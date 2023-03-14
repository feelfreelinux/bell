#include <unistd.h>
#include <arpa/inet.h>

#ifdef BELL_USE_BONJOUR
#include <dns_sd.h>
#else
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#endif

#include "BellLogger.h"
#include "MDNSService.h"

using namespace bell;

#ifndef BELL_USE_BONJOUR
static AvahiClient *avahiClient = NULL;
static AvahiSimplePoll *avahiPoll = NULL;
static void groupHandler(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) { }    
#endif


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
#ifdef BELL_USE_BONJOUR
    const char* service = "_spotify-connect._tcp";
    DNSServiceRef ref = NULL;
    TXTRecordRef txtRecord;
    TXTRecordCreate(&txtRecord, 0, NULL);
  
    for (auto& [key, value] : txtData) {
        TXTRecordSetValue(&txtRecord, key.c_str(), value.size(), value.c_str());
    }
    
    std::string type(serviceType + "." + serviceProto);
    DNSServiceRegister(&ref, 0, 0, serviceName.c_str(), type.c_str(), NULL, NULL, htons(servicePort), 
                       TXTRecordGetLength(&txtRecord), TXTRecordGetBytesPtr(&txtRecord), NULL, NULL);
    TXTRecordDeallocate(&txtRecord);
    
    return ref;
#else
    AvahiEntryGroup *group;
    
    if (!avahiPoll && (avahiPoll = avahi_simple_poll_new()) == NULL) {
        BELL_LOG(error, "MDNS", "cannot create poll for %s", serviceName);
        return NULL;
    }
    
    if (!avahiClient && (avahiClient = avahi_client_new(avahi_simple_poll_get(avahiPoll), 
                                                        AvahiClientFlags(0), NULL, NULL, NULL)) == NULL) {
        BELL_LOG(error, "MDNS", "cannot create client for %s", serviceName);
        return NULL;
    }
    
    if ((group = avahi_entry_group_new(avahiClient, groupHandler, NULL)) == NULL) {
        BELL_LOG(error, "MDNS", "cannot create service %s", serviceName);
        return NULL;
    }  
    
    AvahiStringList* avahiTxt = NULL;  
    for (auto& [key, value] : txtData) {
        BELL_LOG(info, "MDNS", "adding %s=%s", key.c_str(), value.c_str());
        avahiTxt = avahi_string_list_add_pair(avahiTxt, key.c_str(), value.c_str());
    }
    
    std::string type(serviceType + "." + serviceProto);
    int ret = avahi_entry_group_add_service_strlst(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, (AvahiPublishFlags) 0, 
                                                   serviceName.c_str(), type.c_str(), NULL, NULL, servicePort, avahiTxt);
    avahi_string_list_free(avahiTxt);
    
    if (ret >= 0) {
        ret = avahi_entry_group_commit(group);
    }
    
    if (ret < 0) {
        BELL_LOG(error, "MDNS", "cannot run service %s", serviceName);
        avahi_entry_group_free(group);
        group = NULL;
    } 
        
    return group;
#endif    
}

void MDNSService::unregisterService(void* service) {
#ifdef BELL_USE_BONJOUR
    DNSServiceRefDeallocate((DNSServiceRef)service);
#else
    avahi_entry_group_free((AvahiEntryGroup*)service);
#endif
}
