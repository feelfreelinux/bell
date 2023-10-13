#include "MDNSBrowser.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stddef.h>  // for NULL
#include <sys/select.h>
#include <sys/types.h>
#include <stdexcept>
#include <utility>  // for pair
#include <atomic>  // for function
#include "BellLogger.h"

#include "dns_sd.h"  // for DNSServiceRef, DNSServiceRefDeallocate, DNS...

using namespace bell;

class implMDNSBrowser : public MDNSBrowser {
 private:
  DNSServiceRef service;
  std::vector<DNSServiceRef> addrResolveRefs;

  int socketFd = -1;
  std::atomic<bool> runDiscovery = false;

 public:
  struct DNSSDRecord {
    std::string regType, serviceName, domain, host, ipv4;
    uint16_t port;

    implMDNSBrowser* parentPtr = NULL;

    bool expired;

    bool operator==(const DNSSDRecord& rhs) {
      return regType == rhs.regType && rhs.serviceName == serviceName &&
             rhs.domain == domain;
    }
  };

  std::vector<DNSSDRecord> pendingRecords;

  void eraseRecord(DNSSDRecord* recordPtr) {
    DNSSDRecord record = *recordPtr;

    auto recordPos =
        std::find(pendingRecords.begin(), pendingRecords.end(), record);

    if (recordPos != pendingRecords.end()) {
      pendingRecords.erase(recordPos);
    }
  }

  void publishRecords() {
    auto previousRecords = discoveredRecords;

    discoveredRecords.clear();

    // Map DNS-SD records to MDNSBrowser records
    for (auto& record : pendingRecords) {
      auto existingRecord = find_if(
          discoveredRecords.begin(), discoveredRecords.end(),
          [record](const Record& s) { return s.fullname == record.host; });

      if (existingRecord != discoveredRecords.end()) {
        existingRecord->addresses.push_back(record.ipv4);
      } else {
        discoveredRecords.push_back({.fullname = record.host,
                                     .host = record.host,
                                     .addresses = {record.ipv4},
                                     .port = record.port});
      }
    }

    if ((recordsCallback != nullptr) &&
        (previousRecords != discoveredRecords)) {
      recordsCallback(discoveredRecords);
    }
  }

  /// Handle DNS-SD responses
  void handleGetAddrInfoReply(DNSSDRecord* record, DNSServiceRef ref,
                              DNSServiceFlags flags, uint32_t interfaceIndex,
                              DNSServiceErrorType errorCode,
                              const char* hostname,
                              const struct sockaddr* address, uint32_t ttl) {
    if (record->expired) {
      eraseRecord(record);
    } else {
      if (errorCode == kDNSServiceErr_NoError) {
        const sockaddr_in* in = (const sockaddr_in*)address;
        record->ipv4.resize(INET_ADDRSTRLEN + 1);
        inet_ntop(AF_INET, &(in->sin_addr), record->ipv4.data(),
                  INET_ADDRSTRLEN);

        record->ipv4.resize(record->ipv4.find('\0'));
      } else {
        eraseRecord(record);
      }
    }

    publishRecords();
  }

  /// Thin shim passing dns-sd C-style callback to a member function
  static void DNSSD_API getAddrInfoReply(
      DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex,
      DNSServiceErrorType errorCode, const char* hostname,
      const struct sockaddr* address, uint32_t ttl, void* context) {
    auto record = reinterpret_cast<DNSSDRecord*>(context);
    if (record && record->parentPtr) {
      record->parentPtr->handleGetAddrInfoReply(record, ref, flags,
                                                interfaceIndex, errorCode,
                                                hostname, address, ttl);
    }
  }

  /// Handle DNS-SD responses
  void handleServiceResolveReply(DNSSDRecord* recordPtr, DNSServiceRef ref,
                                 DNSServiceFlags flags, uint32_t interfaceIndex,
                                 DNSServiceErrorType errorCode,
                                 const char* fullName, const char* hostTarget,
                                 uint16_t opaqueport, uint16_t txtLen,
                                 const unsigned char* txtRecord) {
    auto refCopy = service;

    if (recordPtr->expired) {
      eraseRecord(recordPtr);
    } else {
      recordPtr->host = fullName;
      auto error = DNSServiceGetAddrInfo(
          &refCopy, kDNSServiceFlagsShareConnection, interfaceIndex,
          kDNSServiceProtocol_IPv4, hostTarget, getAddrInfoReply, recordPtr);

      if (error != kDNSServiceErr_NoError) {
        eraseRecord(recordPtr);
      }
    }
  }

  /// Thin shim passing dns-sd C-style callback to a member function
  static void DNSSD_API serviceResolveReply(
      DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex,
      DNSServiceErrorType errorCode, const char* fullName,
      const char* hostTarget, uint16_t opaqueport, uint16_t txtLen,
      const unsigned char* txtRecord, void* context) {
    auto record = reinterpret_cast<DNSSDRecord*>(context);
    if (record && record->parentPtr) {
      record->port = ntohs(opaqueport);

      record->parentPtr->handleServiceResolveReply(
          record, ref, flags, interfaceIndex, errorCode, fullName, hostTarget,
          opaqueport, txtLen, txtRecord);
    }
  }

  /// Handle DNS-SD responses
  void handleReply(DNSServiceFlags flags, uint32_t interfaceIndex,
                   DNSServiceErrorType errorCode, const char* serviceName,
                   const char* regType, const char* replyDomain) {
    DNSSDRecord record = {
        .regType = regType,
        .serviceName = serviceName,
        .domain = replyDomain,
        .port = 0,
        .parentPtr = this,
        .expired = false,
    };

    auto existingRecord =
        std::find(pendingRecords.begin(), pendingRecords.end(), record);

    if (flags & kDNSServiceFlagsAdd) {
      if (existingRecord == pendingRecords.end()) {
        pendingRecords.push_back(record);
      }

      auto refCopy = service;

      DNSServiceErrorType err =
          DNSServiceResolve(&refCopy, kDNSServiceFlagsShareConnection,
                            interfaceIndex, serviceName, regType, replyDomain,
                            serviceResolveReply, &(pendingRecords.back()));

      if (err != kDNSServiceErr_NoError) {
        BELL_LOG(error, "MDNSBrowser",
                 "Could not start discovered record service resolve, %d", err);
      }
    } else {
      // Either mark record for removal after address resolvation finishes, or remove it now
      if (existingRecord != pendingRecords.end())
        existingRecord->expired = true;

      if (existingRecord->ipv4.length() > 0) {
        pendingRecords.erase(existingRecord);
        publishRecords();
      }
    }

    if (!(flags & kDNSServiceFlagsMoreComing) && recordsCallback) {}
  }

  /// Thin shim passing dns-sd C-style callback to a member function
  static void DNSSD_API browseReply(DNSServiceRef ref, DNSServiceFlags flags,
                                    uint32_t interfaceIndex,
                                    DNSServiceErrorType errorCode,
                                    const char* serviceName,
                                    const char* regType,
                                    const char* replyDomain, void* context) {
    reinterpret_cast<implMDNSBrowser*>(context)->handleReply(
        flags, interfaceIndex, errorCode, serviceName, regType, replyDomain);
  }

  implMDNSBrowser(std::string type, RecordsUpdatedCallback callback) {
    recordsCallback = callback;
    DNSServiceCreateConnection(&service);

    auto refCopy = service;
    auto err = DNSServiceBrowse(&refCopy, kDNSServiceFlagsShareConnection, 0,
                                type.c_str(), nullptr, browseReply, this);
    if (err != kDNSServiceErr_NoError) {
      throw std::runtime_error("Could not initiate the MDNSBrowser (dns-sd)" +
                               std::to_string(err));
    }

    socketFd = DNSServiceRefSockFD(service);
    if (socketFd == -1) {
      throw std::runtime_error("MDNS browser could not bind to a socket");
    }
  }

  /// Closes socket and deallocates dns-sd reference
  void stopDiscovery() {
    runDiscovery = false;

    if (socketFd > -1) {
      ::close(socketFd);
    }
    if (service) {
      DNSServiceRefDeallocate(service);
    }
  }

  void processEvents() {
    runDiscovery = true;

    fd_set readFds;
    int nFds = socketFd + 1;

    // Select and handle data on discovery socket
    while (runDiscovery) {
      FD_ZERO(&readFds);
      FD_SET(socketFd, &readFds);

      struct timeval tv;
      tv.tv_sec = 1;
      tv.tv_usec = 0;

      int result = ::select(nFds, &readFds, (fd_set*)NULL, (fd_set*)NULL, &tv);
      if (result > 0) {
        DNSServiceErrorType err = kDNSServiceErr_NoError;
        if (FD_ISSET(socketFd, &readFds))
          err = DNSServiceProcessResult(service);
        if (err)
          runDiscovery = false;
      } else if (result < 0) {
        BELL_LOG(error, "MDNSBrowser", "select(  ) returned %d errno %d %s\n",
                 result, errno, strerror(errno));
        if (errno != EINTR)
          runDiscovery = false;
      }
    }
  }
};

/**
 * MacOS implementation of MDNSBrowser.
 * @see https://developer.apple.com/documentation/dnssd/1804733-dnsserviceregister
 **/
std::unique_ptr<MDNSBrowser> MDNSBrowser::startDiscovery(
    std::string type, RecordsUpdatedCallback callback) {

  return std::make_unique<implMDNSBrowser>(type, callback);
}
