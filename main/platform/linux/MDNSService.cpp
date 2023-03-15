#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>

#if __has_include("avahi-client/client.h")
#define BELL_HAS_AVAHI  1
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#endif

#include "mdnssvc.h"
#include "BellLogger.h"
#include "MDNSService.h"

using namespace bell;

#ifdef BELL_HAS_AVAHI
static AvahiClient *avahiClient = NULL;
static AvahiSimplePoll *avahiPoll = NULL;
static void groupHandler(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) { }
#endif

static in_addr_t host = INADDR_ANY;
static struct mdnsd *mdnsServer;

/**
 * Linux implementation of MDNSService using avahi.
 * @see https://www.avahi.org/doxygen/html/
 **/
void* MDNSService::registerService(
    const std::string& serviceName,
    const std::string& serviceType,
    const std::string& serviceProto,
    const std::string& serviceHost,
    int servicePort,
    const std::map<std::string, std::string> txtData
) {
#ifdef BELL_HAS_AVAHI
    // try avahi first if available
    if (!avahiPoll) {
       avahiPoll = avahi_simple_poll_new();
    }

    if (avahiPoll && !avahiClient) {
       avahiClient = avahi_client_new(avahi_simple_poll_get(avahiPoll),
                                      AvahiClientFlags(0), NULL, NULL, NULL);
    }

    AvahiEntryGroup *avahiGroup;

    if (avahiClient &&
        (avahiGroup = avahi_entry_group_new(avahiClient, groupHandler, NULL)) == NULL) {
        BELL_LOG(error, "MDNS", "cannot create service %s", serviceName.c_str());
    }

    if (avahiGroup) {
        AvahiStringList* avahiTxt = NULL;

        for (auto& [key, value] : txtData) {
            avahiTxt = avahi_string_list_add_pair(avahiTxt, key.c_str(), value.c_str());
        }

        std::string type(serviceType + "." + serviceProto);
        int ret = avahi_entry_group_add_service_strlst(avahiGroup, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, (AvahiPublishFlags) 0,
                                                       serviceName.c_str(), type.c_str(), NULL, NULL, servicePort, avahiTxt);
        avahi_string_list_free(avahiTxt);

        if (ret >= 0) {
            ret = avahi_entry_group_commit(avahiGroup);
        }

        if (ret < 0) {
            BELL_LOG(error, "MDNS", "cannot run service %s", serviceName.c_str());
            avahi_entry_group_free(avahiGroup);
        } else {
            BELL_LOG(info, "MDNS", "using avahi for %s", serviceName.c_str());
            return avahiGroup;
        }
    }
#endif

    // avahi failed, use build-in server
    struct ifaddrs* ifaddr;

    // get the host address first
    if (serviceHost.size()) {
        struct hostent *h = gethostbyname(serviceHost.c_str());
        if (h) {
            memcpy(&host, h->h_addr_list[0], 4);
        }
    }

    // try go guess ifaddr if we have nothing as listening to INADDR_ANY usually does not work
	if (host == INADDR_ANY && getifaddrs(&ifaddr) != -1) {
        for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET ||
                !(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_MULTICAST) ||
                (ifa->ifa_flags & IFF_LOOPBACK)) continue;

            host = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr;
            break;
        }
        freeifaddrs(ifaddr);
	}

    if (!mdnsServer) {
        char hostname[256];
        struct in_addr addr;

        // it's the same, but who knows..
        addr.s_addr = host;
        gethostname(hostname, sizeof(hostname));

        mdnsServer = mdnsd_start(addr, false);

        if (mdnsServer) {
            mdnsd_set_hostname(mdnsServer, hostname, addr);
        }
    }

    if (mdnsServer) {
        std::vector<const char*> txt;
        std::vector<std::unique_ptr<std::string>> txtStr;

        for (auto& [key, value] : txtData) {
            auto str = make_unique<std::string>(key + "=" + value);
            txtStr.push_back(std::move(str));
            txt.push_back(txtStr.back()->c_str());
        }

        txt.push_back(NULL);
        std::string type(serviceType + "." + serviceProto + ".local");

        BELL_LOG(info, "MDNS", "using build-in mDNS for %s", serviceName.c_str());
        struct mdns_service* mdnsService = mdnsd_register_svc(mdnsServer, serviceName.c_str(),
                                                              type.c_str(), servicePort, NULL, txt.data());
        if (mdnsService) {
            return mdnsService;
        }
    }

    BELL_LOG(error, "MDNS", "cannot start any mDNS listener for %s", serviceName.c_str());
    return NULL;
}

void MDNSService::unregisterService(void* service) {
#ifdef BELL_HAS_AVAHI
    if (avahiClient) {
        avahi_entry_group_free((AvahiEntryGroup*)service);
    } else         
#endif
    {
        mdns_service_remove(mdnsServer, (mdns_service*)service);
    }
}
