#include <vector>
#include <cassert>

#include "MDNSService.h"
#include "BellLogger.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#include "mdnssvc.h"
#else
#include <arpa/inet.h>
#include "mdns.h"
#include <arpa/inet.h>
#endif

using namespace bell;

static struct mdnsd *mdnsService;

/**
 * Win32 implementation of MDNSService
 **/

void* MDNSService::registerService(
    const std::string& serviceName,
    const std::string& serviceType,
    const std::string& serviceProto,
    const std::string& serviceHost,
    int servicePort,
    const std::map<std::string, std::string> txtData
) {
    if (!mdnsService) {
        char hostname[128];
        gethostname(hostname, sizeof(hostname));

        struct sockaddr_in* host = NULL;
        ULONG size = sizeof(IP_ADAPTER_ADDRESSES) * 64;
        IP_ADAPTER_ADDRESSES* adapters = (IP_ADAPTER_ADDRESSES*)malloc(size);
        int ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, 0, adapters, &size);

        for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter && !host; adapter = adapter->Next) {
            if (adapter->TunnelType == TUNNEL_TYPE_TEREDO) continue;
            if (adapter->OperStatus != IfOperStatusUp) continue;

            for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast;
                unicast = unicast->Next) {
                if (adapter->FirstGatewayAddress && unicast->Address.lpSockaddr->sa_family == AF_INET) {
                    host = (struct sockaddr_in*)unicast->Address.lpSockaddr;
                    BELL_LOG(info, "mdns", "mDNS on interface %s", inet_ntoa(host->sin_addr));
                    mdnsService = mdnsd_start(host->sin_addr, false);
                    break;
                }
            }
        }

        assert(mdnsService);
        mdnsd_set_hostname(mdnsService, hostname, host->sin_addr);
        free(adapters);
    }

    std::vector<const char*> txt;
    std::vector<std::unique_ptr<std::string>> txtStr;

    for (auto& [key, value] : txtData) {
        auto str = make_unique<std::string>(key + "=" + value);
        txtStr.push_back(std::move(str));
        txt.push_back(txtStr.back()->c_str());
    }
    txt.push_back(NULL);

    std::string type(serviceType + "." + serviceProto + ".local");
    return mdnsd_register_svc(mdnsService, serviceName.c_str(), type.c_str(), servicePort, NULL, txt.data());
}

void MDNSService::unregisterService(void* service) {
    mdns_service_remove(mdnsService, (mdns_service*)service);
}
