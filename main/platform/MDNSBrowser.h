#pragma once

#include <BellLogger.h>
#include <functional>     // for function
#include <memory>         // for unique_ptr
#include <string>         // for string
#include <unordered_map>  // for unordered_map
#include <vector>         // for vector

namespace bell {

class MDNSBrowser {
 public:
  virtual ~MDNSBrowser() {}

  // Type for MDNS record
  struct DiscoveredRecord {
    std::string name, type, domain, hostname;

    std::vector<std::string> ipv4, ipv6;
    std::unordered_map<std::string, std::string> txtRecords;
    uint16_t port = 0;

    bool operator==(const DiscoveredRecord& rhs) const {
      return name == rhs.name && type == rhs.type && domain == rhs.domain &&
             port == rhs.port && ipv4 == rhs.ipv4;
    }

    bool operator<(const DiscoveredRecord& other) const {
      return name < other.name;
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

 protected:
  std::unordered_map<std::string, std::string> parseMDNSTXTRecord(
      int txtLen, const unsigned char* txtRecord) {
    std::unordered_map<std::string, std::string> result;
    int i = 0;

    while (i < txtLen) {
      // Get the length of the current key-value pair
      int length = txtRecord[i];
      i++;

      if (i + length > txtLen) {
        // This should not happen if the input is well-formed
        BELL_LOG(error, "MDNSBrowser", "Cannot parse ill-formed txt record");
        break;
      }

      // Extract the key-value pair
      std::string pair(reinterpret_cast<const char*>(txtRecord + i), length);
      i += length;

      // Find the position of the '=' character
      size_t pos = pair.find('=');
      if (pos != std::string::npos) {
        // Split into key and value
        std::string key = pair.substr(0, pos);
        std::string value = pair.substr(pos + 1);
        result[key] = value;
      } else {
        // If there's no '=', then it's just a key with an empty value
        result[pair] = "";
      }
    }

    return result;
  }
};

}  // namespace bell