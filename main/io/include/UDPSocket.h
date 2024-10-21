#pragma once

// System includes
#include <arpa/inet.h>
#include <sys/types.h>
#include <cstring>
#include <string>
#include "SocketUtils.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "win32shim.h"
#else
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __sun
#include <sys/filio.h>
#endif
#endif

// Library includes
#include <BellLogger.h>
#include "BellSocket.h"

namespace bell {
class UDPSocket : public bell::Socket {
 private:
  int sockFd{};
  bool isClosed = true;
  SocketUtils::ResolvedAddress addr;

 public:
  UDPSocket() = default;
  ~UDPSocket() { close(); };

  int getFd() override { return sockFd; }

  void open(const std::string& host, uint16_t port) override {
    addr = SocketUtils::resolveDomain(host, SOCK_DGRAM);
    addr.setPort(port);

    // Create the UDP socket (IPv4 or IPv6 depending on the resolution)
    sockFd = socket(addr.family, SOCK_DGRAM, 0);
    if (sockFd < 0) {
      BELL_LOG(error, "udp", "Could not connect to %s. Error %d", host.c_str(),
               errno);
      throw std::runtime_error("Socket creation failed");
    }

    // Set socket options (TOS, buffer size, etc.)
    int iptos = 0x10;
    if (setsockopt(sockFd, IPPROTO_IP, IP_TOS, &iptos, sizeof(iptos)) < 0) {
      BELL_LOG(error, "udp", "Cannot set IP_TOS for UDP socket");
    }

#ifdef __APPLE__
    int nt = NET_SERVICE_TYPE_VO;
    if (setsockopt(sockFd, SOL_SOCKET, SO_NET_SERVICE_TYPE, &nt, sizeof(nt)) <
        0) {
      BELL_LOG(error, "udp", "Cannot set SO_NET_SERVICE_TYPE for UDP socket");
    }
#endif

    int sndBufSize = 16 * 1024;
    if (setsockopt(sockFd, SOL_SOCKET, SO_SNDBUF, &sndBufSize, sizeof(int)) ==
        -1) {
      BELL_LOG(error, "udp", "Setting send buffer size failed");
    }

    isClosed = false;
  }

  void wrapFd(int fd) override {
    if (fd != -1) {
      sockFd = fd;

      isClosed = false;
    }
  }

  void copyAddr(struct sockaddr_in* otherAddr) {
    memcpy(&addr, otherAddr, sizeof(addr));
  }

  size_t read(uint8_t* buf, size_t len) override {
    socklen_t addrlen = addr.addrLen;
    return recvfrom(sockFd, reinterpret_cast<char*>(buf), len, 0,
                    reinterpret_cast<struct sockaddr*>(&addr.addr), &addrlen);
  }

  size_t write(const uint8_t* buf, size_t len) override {
    ssize_t result =
        sendto(sockFd, reinterpret_cast<const char*>(buf), len, 0,
               reinterpret_cast<struct sockaddr*>(&addr.addr), addr.addrLen);

    if (result < 0) {
      BELL_LOG(error, "UDPSocket", "sendto failed, errno=%s", strerror(errno));
      throw std::runtime_error("Error occured while writing to socket");
    }

    return result;
  }

  size_t poll() override {
#ifdef _WIN32
    unsigned long value;
    ioctlsocket(sockFd, FIONREAD, &value);
#else
    int value;
    ioctl(sockFd, FIONREAD, &value);
#endif
    return value;
  }

  bool isOpen() override { return !isClosed; }

  void close() override {
    if (!isClosed) {
#ifdef _WIN32
      closesocket(sockFd);
#else
      ::close(sockFd);
#endif
      sockFd = -1;
      isClosed = true;
    }
  }
};

}  // namespace bell