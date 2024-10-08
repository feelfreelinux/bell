#include "MDNSBrowser.h"

// Currently avahi-only
#ifndef BELL_DISABLE_AVAHI

#include <arpa/inet.h>
#include <avahi-common/address.h>
#include <avahi-common/defs.h>
#include <netinet/in.h>
#include <stddef.h>  // for NULL
#include <sys/select.h>
#include <sys/types.h>
#include <atomic>  // for function
#include <stdexcept>
#include <utility>  // for pair
#include "BellLogger.h"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>

using namespace bell;

class implMDNSBrowser : public MDNSBrowser {
 private:
  AvahiServiceBrowser* avahiSB;
  AvahiClient* avahiClient;
  AvahiSimplePoll* avahiPoll;

 public:
  void publishDiscovered() {
    auto fullyDiscovered = std::vector<DiscoveredRecord>();

    for (auto service : discoveredRecords) {
      if (service.port != 0) {
        fullyDiscovered.push_back(service);
      }
    }

    if (fullyDiscovered.size() > 0 && recordsCallback) {
      recordsCallback(fullyDiscovered);
    }
  }

  static void clientCallback(AvahiClient* c, AvahiClientState state,
                             void* userData) {}

  void resolveCallback(AvahiServiceResolver* r, AvahiIfIndex interface,
                       AvahiProtocol protocol, AvahiResolverEvent event,
                       const char* name, const char* type, const char* domain,
                       const char* host_name, const AvahiAddress* address,
                       uint16_t port, AvahiStringList* txt,
                       AvahiLookupResultFlags flags) {
    switch (event) {
      case AVAHI_RESOLVER_FAILURE:
        fprintf(stderr,
                "(Resolver) Failed to resolve service '%s' of type '%s' in "
                "domain '%s': %s\n",
                name, type, domain,
                avahi_strerror(
                    avahi_client_errno(avahi_service_resolver_get_client(r))));
        break;
      case AVAHI_RESOLVER_FOUND: {
        if (protocol == AVAHI_PROTO_INET) {
          char a[AVAHI_ADDRESS_STR_MAX], *t;
          avahi_address_snprint(a, sizeof(a), address);

          for (auto& record : discoveredRecords) {
            if (record.name == name && record.domain == domain &&
                record.type == type) {
              record.hostname = host_name;
              record.port = port;
              record.ipv4.push_back(a);
            }
          }
        }

        publishDiscovered();
      }
    }
    avahi_service_resolver_free(r);
  }
  // Wraps avahi c style callback
  static void avahiResolveCallback(
      AvahiServiceResolver* r, AvahiIfIndex interface, AvahiProtocol protocol,
      AvahiResolverEvent event, const char* name, const char* type,
      const char* domain, const char* host_name, const AvahiAddress* address,
      uint16_t port, AvahiStringList* txt, AvahiLookupResultFlags flags,
      void* userdata) {
    reinterpret_cast<implMDNSBrowser*>(userdata)->resolveCallback(
        r, interface, protocol, event, name, type, domain, host_name, address,
        port, txt, flags);
  }

  void browseCallback(AvahiIfIndex interface, AvahiProtocol protocol,
                      AvahiBrowserEvent event, const char* name,
                      const char* type, const char* domain,
                      AvahiLookupResultFlags flags) {

    switch (event) {
      case AVAHI_BROWSER_FAILURE:
        fprintf(stderr, "(Browser) %s\n",
                avahi_strerror(avahi_client_errno(avahiClient)));
        avahi_simple_poll_quit(avahiPoll);
        return;
      case AVAHI_BROWSER_NEW: {
        DiscoveredRecord record = {
            .name = name,
            .type = type,
            .domain = domain,
            .hostname = "",
            .ipv4 = {},
            .ipv6 = {},
            .port = 0,
        };

        if (std::find(discoveredRecords.begin(), discoveredRecords.end(),
                      record) == discoveredRecords.end() &&
            protocol == AVAHI_PROTO_INET) {
          discoveredRecords.push_back(record);

          if (!(avahi_service_resolver_new(
                  avahiClient, interface, protocol, name, type, domain,
                  AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)0, avahiResolveCallback,
                  this)))
            BELL_LOG(error, "Failed to resolve service '%s': %s", name,
                     avahi_strerror(avahi_client_errno(avahiClient)));
        }

        break;
      }
      case AVAHI_BROWSER_REMOVE:
        for (auto iter = discoveredRecords.begin();
             iter != discoveredRecords.end();) {
          if (iter->name == name && iter->type == type &&
              iter->domain == domain)
            iter = discoveredRecords.erase(iter);
          else
            ++iter;
        }
        publishDiscovered();
        break;
      case AVAHI_BROWSER_ALL_FOR_NOW:
      case AVAHI_BROWSER_CACHE_EXHAUSTED:
        break;
    }
  }

  // Wraps avahi c style callback
  static void avahiBrowseCallback(
      AvahiServiceBrowser* b, AvahiIfIndex interface, AvahiProtocol protocol,
      AvahiBrowserEvent event, const char* name, const char* type,
      const char* domain, AvahiLookupResultFlags flags, void* userdata) {
    reinterpret_cast<implMDNSBrowser*>(userdata)->browseCallback(
        interface, protocol, event, name, type, domain, flags);
  }

  implMDNSBrowser(std::string type, RecordsUpdatedCallback callback) {
    recordsCallback = callback;
    int error;

    avahiPoll = avahi_simple_poll_new();
    if (avahiPoll == NULL) {
      throw std::runtime_error("Could not allocate poll");
    }

    avahiClient =
        avahi_client_new(avahi_simple_poll_get(avahiPoll), (AvahiClientFlags)0,
                         clientCallback, NULL, &error);
    if (avahiClient == NULL) {
      BELL_LOG(error, "MDNSBrowser", "Could not create an client, err=%s", avahi_strerror(error));
      throw std::runtime_error("Could not create an client");
    }

    avahiSB = avahi_service_browser_new(
        avahiClient, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, type.c_str(), NULL,
        (AvahiLookupFlags)0, avahiBrowseCallback, this);
    if (avahiSB == NULL) {
      throw std::runtime_error("Could not create an browser");
    }
  }

  /// Closes socket and deallocates dns-sd reference
  void stopDiscovery() {}

  void processEvents() { avahi_simple_poll_loop(avahiPoll); }
};

/**
 * MacOS implementation of MDNSBrowser.
 **/
std::unique_ptr<MDNSBrowser> MDNSBrowser::startDiscovery(
    std::string type, RecordsUpdatedCallback callback) {

  return std::make_unique<implMDNSBrowser>(type, callback);
}
#endif