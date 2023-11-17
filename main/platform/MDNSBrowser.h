#pragma once

#include <functional>  // for function
#include <map>         // for map
#include <memory>      // for unique_ptr
#include <string>      // for string
#include <vector>      // for vector

namespace bell {

class MDNSBrowser {
 public:
  virtual ~MDNSBrowser() {}

  // Type for MDNS record
  struct DiscoveredRecord {
    std::string name, type, domain, hostname;

    std::vector<std::string> ipv4, ipv6;
    uint16_t port = 0;

    bool operator==(const DiscoveredRecord& rhs) const {
      return name == rhs.name && type == rhs.type && domain == rhs.domain;
    }
  };

  // Current list of discovered devices
  std::vector<DiscoveredRecord> discoveredRecords;

  // Typedef for record callback
  using RecordsUpdatedCallback =
      std::function<void(std::vector<DiscoveredRecord>&)>;

  // Holds the callback
  RecordsUpdatedCallback recordsCallback;

  // Needs to be called to perform service discovery
  virtual void processEvents() = 0;

  // stops discovery
  virtual void stopDiscovery() = 0;

  // Starts discovery of passed type and returns implemented browser instance
  static std::unique_ptr<MDNSBrowser> startDiscovery(
      std::string type, RecordsUpdatedCallback callback);
};

}  // namespace bell