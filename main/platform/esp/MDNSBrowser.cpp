#include "MDNSBrowser.h"
#include <set>
#include <stdexcept>
#include "BellLogger.h"
#include "esp_netif_ip_addr.h"
#include "lwip/ip_addr.h"
#include "mdns.h"

using namespace bell;

class implMDNSBrowser : public MDNSBrowser {
 private:
  std::string serviceName, proto;
  std::set<DiscoveredRecord> lastDiscoveredDevices;

 public:
  void publishDiscovered() {
    auto fullyDiscovered = std::set<DiscoveredRecord>();

    for (auto service : discoveredRecords) {
      if (service.port != 0) {
        fullyDiscovered.insert(service);
      }
    }

    if (fullyDiscovered.size() > 0 && recordsCallback &&
        fullyDiscovered != lastDiscoveredDevices) {
      std::vector<DiscoveredRecord> res(fullyDiscovered.begin(),
                                        fullyDiscovered.end());
      recordsCallback(res);
    }

    // Only notify on state changes
    lastDiscoveredDevices = fullyDiscovered;
  }

  implMDNSBrowser(std::string type, RecordsUpdatedCallback callback) {
    recordsCallback = callback;
    auto delimiterPos = type.find(".");
    serviceName = type.substr(0, delimiterPos);
    proto = type.substr(delimiterPos + 1);
  }
  void stopDiscovery() {}

  void processResults(mdns_result_t* results) {
    mdns_result_t* r = results;
    mdns_ip_addr_t* a = NULL;

    discoveredRecords.clear();

    while (r) {
      DiscoveredRecord record;

      if (r->instance_name) {
        record.name = r->instance_name;
      }

      if (r->hostname) {
        record.hostname = r->hostname;
        record.port = r->port;
      }

      a = r->addr;
      while (a) {
        if (a->addr.type == IPADDR_TYPE_V4) {
          char strIp[16];
          esp_ip4addr_ntoa(&a->addr.u_addr.ip4, strIp, IP4ADDR_STRLEN_MAX);
          record.ipv4.push_back(std::string(strIp));
        }
        a = a->next;
      }

      for (auto& other : discoveredRecords) {
        if (record.hostname == other.hostname && record.ipv4.empty()) {
          record.ipv4 = other.ipv4;
        }
      }

      // copy txt records int std::unordered_map
      for (int x = 0; x < r->txt_count; x++) {
        record.txtRecords.insert(
            {std::string(r->txt[x].key),
             std::string(&r->txt[x].value[0],
                         &r->txt[x].value[r->txt_value_len[x]])});
      }

      discoveredRecords.push_back(record);
      r = r->next;
    }

    publishDiscovered();
  }

  void processEvents() {
    mdns_result_t* results = NULL;
    esp_err_t err = mdns_query_ptr(this->serviceName.c_str(),
                                   this->proto.c_str(), 3000, 32, &results);
    if (err) {
      throw std::runtime_error("Could not query mdns services");
    }

    processResults(results);

    mdns_query_results_free(results);
  }
};

/**
 * MacOS implementation of MDNSBrowser.
 **/
std::unique_ptr<MDNSBrowser> MDNSBrowser::startDiscovery(
    std::string type, RecordsUpdatedCallback callback) {

  return std::make_unique<implMDNSBrowser>(type, callback);
}
