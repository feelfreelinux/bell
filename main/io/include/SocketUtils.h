#pragma once

#include <sys/socket.h>
#include <string>

namespace bell::SocketUtils {
struct ResolvedAddress {
  sockaddr_storage addr;  // Can store either IPv4 or IPv6 address
  socklen_t addrLen;      // Length of the sockaddr structure
  int family;             // Address family: AF_INET or AF_INET6

  void setPort(int port);
};

ResolvedAddress resolveDomain(const std::string& hostname, int sockType);
};  // namespace bell::SocketUtils