#pragma once

#include <map>     // for map
#include <memory>  // for unique_ptr
#include <string>  // for string
#include <vector>  // for vector
#include <functional>  // for function

namespace bell {

class MDNSBrowser {
 public:
  virtual ~MDNSBrowser() {}

  // Type for MDNS record
  struct Record {
    std::string fullname, host;

    std::vector<std::string> addresses;
    uint16_t port = 0;

    bool operator==(const Record& rhs) const {
      return fullname == rhs.fullname && port == rhs.port &&
             addresses == rhs.addresses;
    }
  };

  // Current list of discovered devices
  std::vector<Record> discoveredRecords;

  // Typedef for record callback
  using RecordsUpdatedCallback = std::function<void(std::vector<Record>&)>;

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