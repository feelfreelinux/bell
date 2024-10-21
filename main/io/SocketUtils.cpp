#include "SocketUtils.h"

// Socket includes
#include <arpa/inet.h>
#include <netdb.h>

#include <stdexcept>

using namespace bell;

// Helper to check if the hostname is an IP and populate sockaddr accordingly
bool isIpAddress(const std::string& hostname,
                 SocketUtils::ResolvedAddress& resolved) {
  struct sockaddr_in ipv4Addr;
  struct sockaddr_in6 ipv6Addr;

  // Try to parse IPv4 address
  if (inet_pton(AF_INET, hostname.c_str(), &(ipv4Addr.sin_addr)) == 1) {
    resolved.family = AF_INET;
    ipv4Addr.sin_family = AF_INET;
    resolved.addrLen = sizeof(struct sockaddr_in);
    memcpy(&resolved.addr, &ipv4Addr, resolved.addrLen);
    return true;
  }

  // Try to parse IPv6 address
  if (inet_pton(AF_INET6, hostname.c_str(), &(ipv6Addr.sin6_addr)) == 1) {
    resolved.family = AF_INET6;
    ipv6Addr.sin6_family = AF_INET6;
    resolved.addrLen = sizeof(struct sockaddr_in6);
    memcpy(&resolved.addr, &ipv6Addr, resolved.addrLen);
    return true;
  }

  // Not an IP address
  return false;
}

void SocketUtils::ResolvedAddress::setPort(int port) {
  if (family == AF_INET) {
    // For IPv4
    sockaddr_in* ipv4Addr = reinterpret_cast<sockaddr_in*>(&addr);
    ipv4Addr->sin_port = htons(port);  // Set port in network byte order
  } else if (family == AF_INET6) {
    // For IPv6
    sockaddr_in6* ipv6Addr = reinterpret_cast<sockaddr_in6*>(&addr);
    ipv6Addr->sin6_port = htons(port);  // Set port in network byte order
  } else {
    throw std::runtime_error("Unsupported address family for port assignment");
  }
}

SocketUtils::ResolvedAddress SocketUtils::resolveDomain(
    const std::string& hostname, int socktype) {
  SocketUtils::ResolvedAddress resolved;

  // First, check if hostname is already an IP address (IPv4 or IPv6)
  if (isIpAddress(hostname, resolved)) {
    return resolved;  // If it is an IP, we already populated `resolved`
  }

  // Otherwise, treat it as a domain name and resolve it
  struct addrinfo hints, *res = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;   // Allow both IPv4 and IPv6
  hints.ai_socktype = socktype;  // TCP (SOCK_STREAM) or UDP (SOCK_DGRAM)

  int result = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
  if (result != 0) {
    throw std::runtime_error("Failed to resolve domain: " +
                             std::string(gai_strerror(result)));
  }

  // We'll use the first valid result
  struct addrinfo* addr = res;

  // Copy over the resolved address into our sockaddr_storage
  if (addr->ai_family == AF_INET) {
    // IPv4
    resolved.family = AF_INET;
    resolved.addrLen = sizeof(struct sockaddr_in);
    memcpy(&resolved.addr, addr->ai_addr, resolved.addrLen);
  } else if (addr->ai_family == AF_INET6) {
    // IPv6
    resolved.family = AF_INET6;
    resolved.addrLen = sizeof(struct sockaddr_in6);
    memcpy(&resolved.addr, addr->ai_addr, resolved.addrLen);
  } else {
    freeaddrinfo(res);
    throw std::runtime_error("Unsupported address family");
  }

  // Clean up
  freeaddrinfo(res);

  return resolved;
}