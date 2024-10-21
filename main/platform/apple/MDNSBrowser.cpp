#include "MDNSBrowser.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stddef.h>  // for NULL
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <atomic>  // for function
#include <stdexcept>
#include <string>
#include <utility>  // for pair
#include "BellLogger.h"
#include "dns_sd.h"  // for DNSServiceRef, DNSServiceRefDeallocate, DNS...

using namespace bell;

class implMDNSBrowser : public MDNSBrowser {
 private:
  DNSServiceRef service;
  std::vector<DNSServiceRef> addrResolveRefs;
  std::vector<DiscoveredRecord> lastPublishedRecords;

  int socketFd = -1;
  fd_set readFds;
  int nFds;

 public:
  struct AddrResolvReference {
    implMDNSBrowser* parentPtr;
    std::string name, type, domain;
    std::unordered_map<std::string, std::string> txtRecords;
    int port = 0;
  };

  void publishDiscovered() {
    auto fullyDiscovered = std::vector<DiscoveredRecord>();

    for (auto service : discoveredRecords) {
      if (service.port != 0) {
        fullyDiscovered.push_back(service);
      }
    }

    if (recordsCallback && (lastPublishedRecords != fullyDiscovered)) {
      recordsCallback(fullyDiscovered);
      lastPublishedRecords = fullyDiscovered;
    }
  }

  /// Handle DNS-SD responses
  void handleGetAddrInfoReply(AddrResolvReference* ctx, DNSServiceRef ref,
                              DNSServiceFlags flags, uint32_t interfaceIndex,
                              DNSServiceErrorType errorCode,
                              const char* hostname,
                              const struct sockaddr* address, uint32_t ttl) {
    if (errorCode == kDNSServiceErr_NoError) {
      const sockaddr_in* in = (const sockaddr_in*)address;
      std::string addrStr;
      addrStr.resize(INET_ADDRSTRLEN + 1);
      inet_ntop(AF_INET, &(in->sin_addr), addrStr.data(), INET_ADDRSTRLEN);

      addrStr.resize(addrStr.find('\0'));

      for (auto& record : discoveredRecords) {
        if (record.name == ctx->name && record.domain == ctx->domain &&
            record.type == ctx->type) {
          record.txtRecords = ctx->txtRecords;
          record.hostname = hostname;
          record.ipv4.push_back(addrStr);
          record.port = ctx->port;
        }
      }

      publishDiscovered();
    }
  }

  /// Thin shim passing dns-sd C-style callback to a member function
  static void DNSSD_API getAddrInfoReply(
      DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex,
      DNSServiceErrorType errorCode, const char* hostname,
      const struct sockaddr* address, uint32_t ttl, void* context) {
    auto ctx = reinterpret_cast<AddrResolvReference*>(context);
    if (ctx->parentPtr != NULL) {
      ctx->parentPtr->handleGetAddrInfoReply(ctx, ref, flags, interfaceIndex,
                                             errorCode, hostname, address, ttl);
    }

    DNSServiceRefDeallocate(ref);
    // Not passed anywhere further
    delete ctx;
  }

  /// Handle DNS-SD responses
  void handleServiceResolveReply(AddrResolvReference* ctx, DNSServiceRef ref,
                                 DNSServiceFlags flags, uint32_t interfaceIndex,
                                 DNSServiceErrorType errorCode,
                                 const char* fullName, const char* hostTarget,
                                 uint16_t opaqueport, uint16_t txtLen,
                                 const unsigned char* txtRecord) {
    auto refCopy = service;
    if (ctx->parentPtr != NULL) {
      ctx->txtRecords = parseMDNSTXTRecord(txtLen, txtRecord);
      auto error = DNSServiceGetAddrInfo(
          &refCopy, kDNSServiceFlagsShareConnection, 0,
          kDNSServiceProtocol_IPv4, hostTarget, getAddrInfoReply, ctx);

      if (error != kDNSServiceErr_NoError) {
        delete ctx;
      }
    } else {
      delete ctx;
    }
  }

  /// Thin shim passing dns-sd C-style callback to a member function
  static void DNSSD_API serviceResolveReply(
      DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex,
      DNSServiceErrorType errorCode, const char* fullName,
      const char* hostTarget, uint16_t opaqueport, uint16_t txtLen,
      const unsigned char* txtRecord, void* context) {
    auto ctx = reinterpret_cast<AddrResolvReference*>(context);
    if (ctx->parentPtr != NULL) {
      ctx->port = ntohs(opaqueport);
      ctx->parentPtr->handleServiceResolveReply(ctx, ref, flags, interfaceIndex,
                                                errorCode, fullName, hostTarget,
                                                opaqueport, txtLen, txtRecord);
    }
    DNSServiceRefDeallocate(ref);
  }

  /// Handle DNS-SD responses
  void handleReply(DNSServiceFlags flags, uint32_t interfaceIndex,
                   DNSServiceErrorType errorCode, const char* serviceName,
                   const char* regType, const char* replyDomain) {
    DiscoveredRecord record = {
        .name = serviceName,
        .type = regType,
        .domain = replyDomain,
        .hostname = "",
        .ipv4 = {},
        .ipv6 = {},
        .port = 0,
    };

    auto existingRecord =
        std::find(discoveredRecords.begin(), discoveredRecords.end(), record);

    if (flags & kDNSServiceFlagsAdd) {
      if (existingRecord == discoveredRecords.end()) {
        discoveredRecords.push_back(record);

        auto refCopy = service;

        auto addrResolvCtx = new AddrResolvReference();

        // Prepare context for dns sd
        addrResolvCtx->domain = record.domain;
        addrResolvCtx->type = record.type;
        addrResolvCtx->name = record.name;
        addrResolvCtx->parentPtr = this;
        DNSServiceErrorType err = DNSServiceResolve(
            &refCopy, kDNSServiceFlagsShareConnection, 0, serviceName, regType,
            replyDomain, serviceResolveReply, addrResolvCtx);

        if (err != kDNSServiceErr_NoError) {
          BELL_LOG(error, "MDNSBrowser",
                   "Could not start discovered record service resolve, %d",
                   err);
        }
      }
    } else {
      for (auto iter = discoveredRecords.begin();
           iter != discoveredRecords.end();) {
        if (iter->name == serviceName && iter->type == regType &&
            iter->domain == replyDomain)
          iter = discoveredRecords.erase(iter);
        else
          ++iter;
      }
      publishDiscovered();
    }

    if (!(flags & kDNSServiceFlagsMoreComing) &&
        (flags & kDNSServiceFlagsAdd)) {
      publishDiscovered();
    }
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

    nFds = socketFd + 1;
  }

  /// Closes socket and deallocates dns-sd reference
  void stopDiscovery() {
    if (socketFd > -1) {
      ::close(socketFd);
    }
    if (service) {
      DNSServiceRefDeallocate(service);
    }
  }

  void processEvents() {
    // Select and handle data on discovery socket
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
        throw std::runtime_error("cannot run dns discovery");
    } else if (result < 0) {
      BELL_LOG(error, "MDNSBrowser", "select(  ) returned %d errno %d %s\n",
               result, errno, strerror(errno));
      if (errno != EINTR)
        throw std::runtime_error("cannot run dns discovery");
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
