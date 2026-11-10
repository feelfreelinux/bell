#include "bell/mdns/Manager.h"

#ifndef BELL_DISABLE_MDNS

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <new>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bell/Logger.h"
#include "bell/Result.h"
#include "bell/mdns/Error.h"
#include "bell/net/IpAddress.h"
#include "bell/utils/Task.h"

#include "esp_log.h"
#include "mdns.h"

using namespace bell::mdns;

namespace {

constexpr const char* dispatcherLogTag = "EspressifMdnsBrowseDispatcher";
constexpr size_t maxQueuedBrowseResults = 64;

void logBrowseDrop(const char* reason) {
  static std::atomic<uint32_t> drops{0};
  const uint32_t total = drops.fetch_add(1, std::memory_order_relaxed) + 1;
  if (total == 1 || (total % 64) == 0) {
    ESP_LOGW(dispatcherLogTag, "Dropped %lu mDNS browse update(s), reason=%s",
             static_cast<unsigned long>(total), reason);
  }
}

struct BrowseResult {
  std::string serviceType;  // "_service._proto", the browse regType
  std::string instanceName;
  std::string hostname;
  uint16_t port = 0;
  uint32_t ttl = 0;
  int netifIndex = 0;
  std::unordered_map<std::string, std::string> txtRecords;
  std::optional<bell::net::IpAddress> ipv4;
};

BrowseResult snapshotResult(mdns_result_t* r) {
  BrowseResult out;
  if (r->service_type) {
    out.serviceType = r->service_type;
  }
  if (r->proto) {
    if (!out.serviceType.empty()) {
      out.serviceType += ".";
    }
    out.serviceType += r->proto;
  }
  if (r->instance_name) {
    out.instanceName = r->instance_name;
  }
  if (r->hostname) {
    out.hostname = r->hostname;
  }
  out.port = r->port;
  out.ttl = r->ttl;
  if (r->esp_netif) {
    out.netifIndex = esp_netif_get_netif_impl_index(r->esp_netif);
  }

  for (size_t x = 0; x < r->txt_count; x++) {
    if (!r->txt[x].key) {
      continue;
    }
    const char* value = r->txt[x].value ? r->txt[x].value : "";
    out.txtRecords.insert({std::string(r->txt[x].key),
                           std::string(value, value + r->txt_value_len[x])});
  }

  for (mdns_ip_addr_t* a = r->addr; a; a = a->next) {
    if (a->addr.type == IPADDR_TYPE_V4) {
      std::array<char, IP4ADDR_STRLEN_MAX> strCharData{};
      esp_ip4addr_ntoa(&a->addr.u_addr.ip4, strCharData.data(),
                       IP4ADDR_STRLEN_MAX);
      out.ipv4 = bell::net::IpAddress::fromString(strCharData.data());
    }
    // TODO: IPv6
  }

  return out;
}

// Points into txtRecords, mdns copies the items
std::vector<mdns_txt_item_t> toTxtItems(
    const std::unordered_map<std::string, std::string>& txtRecords) {
  std::vector<mdns_txt_item_t> items;
  items.reserve(txtRecords.size());
  for (const auto& [key, value] : txtRecords) {
    items.push_back({.key = key.c_str(), .value = value.c_str()});
  }
  return items;
}

// Runs browse callbacks on a bell task, off the mdns service task. Handlers
// run under dispatchMutex, so they must not register or unregister
class BrowseDispatcher : public bell::Task {
 public:
  BrowseDispatcher()
      : bell::Task("mdns_browse_dispatch", 1024 * 8, 1,
                   bell::utils::TaskCore::Core0, /*espStackOnPsram=*/true) {
    if (startTask()) {
      activeDispatcher.store(this, std::memory_order_release);
    } else {
      ESP_LOGE(dispatcherLogTag, "Failed to start mDNS browse dispatcher task");
    }
  }

  ~BrowseDispatcher() {
    BrowseDispatcher* self = this;
    activeDispatcher.compare_exchange_strong(self, nullptr);
    stopTask();
  }

  using ResultHandler = std::function<void(const BrowseResult&)>;

  // One handler per type, the latest registration owns it
  void registerHandler(const std::string& regType, const void* owner,
                       ResultHandler handler) {
    std::scoped_lock lock(dispatchMutex);
    handlers[regType] = {owner, std::move(handler)};
  }

  // Removed only while owner holds it, so an old browser leaves a newer one's
  // handler alone. Waits out an in-flight dispatch
  bool unregisterHandler(const std::string& regType, const void* owner) {
    std::scoped_lock lock(dispatchMutex);
    auto it = handlers.find(regType);
    if (it == handlers.end() || it->second.owner != owner)
      return false;
    handlers.erase(it);
    return true;
  }

  // On the mdns task. Bounded, an unbounded backlog starves the mdns task of
  // heap and stalls its action queue
  void enqueue(BrowseResult&& result) {
    {
      std::scoped_lock lock(queueMutex);
      if (queue.size() >= maxQueuedBrowseResults) {
        queue.pop_front();
        logBrowseDrop("dispatcher backlog");
      }
      queue.push_back(std::move(result));
    }
    queueCv.notify_one();
  }

  static inline std::atomic<BrowseDispatcher*> activeDispatcher{nullptr};

 protected:
  void wakeTask() override {
    {
      std::scoped_lock lock(queueMutex);
      wakeSignal = true;
    }
    queueCv.notify_one();
  }

  void taskLoop() override {
    BrowseResult result;
    {
      std::unique_lock lock(queueMutex);
      queueCv.wait(lock, [this] { return wakeSignal || !queue.empty(); });
      if (queue.empty()) {
        // stopTask() wake
        wakeSignal = false;
        return;
      }
      result = std::move(queue.front());
      queue.pop_front();
    }

    std::scoped_lock lock(dispatchMutex);
    auto it = handlers.find(result.serviceType);
    if (it != handlers.end()) {
      it->second.handler(result);
    }
  }

 private:
  std::mutex queueMutex;
  std::condition_variable queueCv;
  bool wakeSignal = false;
  std::deque<BrowseResult> queue;

  struct OwnedHandler {
    const void* owner = nullptr;
    ResultHandler handler;
  };
  std::mutex dispatchMutex;
  std::unordered_map<std::string, OwnedHandler> handlers;
};

// One result per call, next points to the sibling
void browseNotifier(mdns_result_t* result) {
  auto* dispatcher =
      BrowseDispatcher::activeDispatcher.load(std::memory_order_acquire);
  if (!dispatcher || !result) {
    return;
  }

  try {
    dispatcher->enqueue(snapshotResult(result));
  } catch (const std::bad_alloc&) {
    logBrowseDrop("out of memory");
  } catch (const std::exception& e) {
    logBrowseDrop(e.what());
  } catch (...) {
    logBrowseDrop("unknown exception");
  }
}

class EspressifMdnsBrowser : public Browser {
 public:
  EspressifMdnsBrowser(std::shared_ptr<BrowseDispatcher> dispatcher)
      : dispatcher(std::move(dispatcher)) {}

  ~EspressifMdnsBrowser() { stopDiscovery(); }

  bell::Result<> browse(const std::string& regType,
                        const DiscoveryEventCallback& onEvent) {
    stopDiscovery();

    this->regType = regType;
    this->regService = regType.substr(0, regType.find_first_of('.'));
    this->regProto = regType.substr(regType.find_first_of('.') + 1);
    this->onEvent = onEvent;

    // Before the browse starts, so none of its results are missed
    dispatcher->registerHandler(regType, this, [this](const BrowseResult& r) {
      this->handleResult(r);
    });

    if (!mdns_browse_new(regService.c_str(), regProto.c_str(),
                         browseNotifier)) {
      BELL_LOG(error, LOG_TAG, "Failed to start mDNS browse for {}", regType);
      dispatcher->unregisterHandler(regType, this);
      this->regType.clear();
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_discovery_failed);
    }

    return {};
  }

  // Resolution is automatic on Espressif
  bell::Result<> resolveService(const ServiceRecord&) override { return {}; }
  bell::Result<> resolveAddress(const ServiceRecord&) override { return {}; }

  void stopDiscovery() override {
    if (!regType.empty()) {
      // Browses are deleted by name, a browser that lost its type leaves the
      // newer browse alone
      if (dispatcher->unregisterHandler(regType, this))
        (void)mdns_browse_delete(regService.c_str(), regProto.c_str());
      regType.clear();
    }
  }

 private:
  static constexpr const char* LOG_TAG = "EspressifMdnsBrowser";

  DiscoveryEventCallback onEvent = {};
  std::shared_ptr<BrowseDispatcher> dispatcher;

  std::string regService;
  std::string regProto;
  std::string regType;  // regService.regProto

  std::set<ServiceRecord> recordsCache{};

  // Folds one browse update into the cache and emits its events
  void handleResult(const BrowseResult& result) {
    if (!onEvent) {
      return;
    }

    ServiceRecord service(result.instanceName, result.serviceType, "",
                          result.netifIndex);

    if (!result.hostname.empty()) {
      service.hostname = result.hostname;
      service.port = result.port;
      service.txtRecords = result.txtRecords;
      service.serviceResolved = true;
    }

    service.ipv4 = result.ipv4;

    if (!service.ipv4 && !service.hostname.empty()) {
      // Espressif mdns can drop the address when a host has two instances of
      // the same service
      for (const auto& cached : recordsCache) {
        if (cached.hostname == service.hostname && cached.ipv4.has_value()) {
          service.ipv4 = cached.ipv4;
          break;
        }
      }
    }

    auto emit = [&](EventType type) {
      onEvent(DiscoveryEvent{.type = type, .service = service, .error = {}});
    };

    auto it = recordsCache.find(service);
    const bool freshRecord = it == recordsCache.end();

    if (freshRecord) {
      if (result.ttl == 0) {
        // Goodbye for a service never reported
        return;
      }
      emit(EventType::Added);
      it = recordsCache.insert(service).first;
    }

    if (result.ttl == 0) {
      emit(EventType::Removed);
      recordsCache.erase(it);
      return;
    }

    bool serviceUpdated = false;
    if ((!it->serviceResolved && service.serviceResolved) ||
        (it->serviceResolved && freshRecord)) {
      emit(EventType::Resolved);
      serviceUpdated = true;
    }

    if ((!it->ipv4.has_value() && service.ipv4.has_value()) ||
        (it->ipv4.has_value() && freshRecord)) {
      emit(EventType::AddressResolved);
      serviceUpdated = true;
    }

    if (serviceUpdated && !freshRecord) {
      recordsCache.erase(it);
      recordsCache.insert(service);
    }
  }
};

class EspressifMdnsAdvertiser : public Advertiser {
 public:
  EspressifMdnsAdvertiser() = default;
  ~EspressifMdnsAdvertiser() override { stopAdvertising(); }

  EspressifMdnsAdvertiser(const EspressifMdnsAdvertiser&) = delete;
  EspressifMdnsAdvertiser& operator=(const EspressifMdnsAdvertiser&) = delete;

  bell::Result<> advertise(
      const std::string& serviceName, const std::string& serviceType,
      uint16_t port,
      const std::unordered_map<std::string, std::string>& txtRecords) {
    regService = serviceType.substr(0, serviceType.find_first_of('.'));
    regProto = serviceType.substr(serviceType.find_first_of('.') + 1);

    auto txtItems = toTxtItems(txtRecords);
    auto res = mdns_service_add(serviceName.c_str(), regService.c_str(),
                                regProto.c_str(), port, txtItems.data(),
                                txtItems.size());

    if (res != ESP_OK) {
      // Nothing to remove or update later
      regService.clear();
      regProto.clear();
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    return {};
  }

  bell::Result<> update(
      const std::string& serviceName,
      const std::unordered_map<std::string, std::string>& txtRecords) override {
    if (regService.empty()) {
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    // Renamed in place, not re-added
    auto res = mdns_service_instance_name_set(
        regService.c_str(), regProto.c_str(), serviceName.c_str());
    if (res != ESP_OK) {
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    auto txtItems = toTxtItems(txtRecords);
    res = mdns_service_txt_set(regService.c_str(), regProto.c_str(),
                               txtItems.data(), txtItems.size());
    if (res != ESP_OK) {
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    return {};
  }

  void stopAdvertising() override {
    if (!regService.empty()) {
      (void)mdns_service_remove(regService.c_str(), regProto.c_str());
      regService.clear();
      regProto.clear();
    }
  }

 private:
  std::string regService;
  std::string regProto;
};

class EspressifMDNSManager : public Manager {
 public:
  EspressifMDNSManager() = default;

  bell::Result<std::unique_ptr<Browser>> browse(
      const std::string& serviceType, const std::string& /*serviceDomain*/,
      int /*interfaceIndex*/, const Browser::DiscoveryEventCallback& onEvent,
      bool /*autoResolveService */, bool /*autoResolveAddresses */,
      bool /*resolveIPv6*/) override {
    auto browser = std::make_unique<EspressifMdnsBrowser>(browseDispatcher);
    auto res = browser->browse(serviceType, onEvent);

    if (!res) {
      return nonstd::make_unexpected(res.error());
    }

    return browser;
  }

  bell::Result<std::unique_ptr<Advertiser>> advertise(
      const std::string& serviceName, const std::string& serviceType,
      const std::string& /*serviceDomain*/, const std::string& /*serviceHost*/,
      uint16_t port,
      const std::unordered_map<std::string, std::string>& txtRecords,
      int /*interfaceIndex*/) override {
    auto advertiser = std::make_unique<EspressifMdnsAdvertiser>();
    auto res =
        advertiser->advertise(serviceName, serviceType, port, txtRecords);

    if (!res) {
      return nonstd::make_unexpected(res.error());
    }

    return advertiser;
  }

  static EspressifMDNSManager* getDefaultManager() {
    // Never destroyed, its dispatcher task runs for the program's lifetime
    static auto* instance = new EspressifMDNSManager();
    return instance;
  }

 private:
  std::shared_ptr<BrowseDispatcher> browseDispatcher =
      std::make_shared<BrowseDispatcher>();
};

}  // namespace

Manager* bell::mdns::getDefaultManager() {
  return EspressifMDNSManager::getDefaultManager();
}

#endif
