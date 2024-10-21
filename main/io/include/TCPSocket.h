#ifndef BELL_BASIC_SOCKET_H
#define BELL_BASIC_SOCKET_H

#include <stdlib.h>
#include <sys/types.h>
#include <cstring>
#include <string>

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

#include <BellLogger.h>
#include "BellSocket.h"
#include "SocketUtils.h"

namespace bell {
class TCPSocket : public bell::Socket {
 private:
  int sockFd;
  bool isClosed = true;

 public:
  TCPSocket(){};
  ~TCPSocket() { close(); };

  int getFd() { return sockFd; }

  void open(const std::string& host, uint16_t port) {
    int err;

    auto resolvedAddress = SocketUtils::resolveDomain(host, SOCK_STREAM);
    resolvedAddress.setPort(port);

    sockFd = socket(resolvedAddress.family, SOCK_STREAM, IPPROTO_IP);

    if (sockFd < 0) {
      BELL_LOG(error, "http",
               "Could not create socket to %s, port %d. Error %d", host.c_str(),
               port, errno);
      throw std::runtime_error("Sock create failed");
    }

    isClosed = false;
    err = connect(sockFd,
                  reinterpret_cast<struct sockaddr*>(&resolvedAddress.addr),
                  resolvedAddress.addrLen);
    if (err < 0) {
      close();
      BELL_LOG(error, "http", "Could not connect to %s, port %d. Error %d",
               host.c_str(), port, errno);
      throw std::runtime_error("Sock connect failed");
    }

    int flag = 1;
    setsockopt(sockFd,       /* socket affected */
               IPPROTO_TCP,  /* set option at TCP level */
               TCP_NODELAY,  /* name of option */
               (char*)&flag, /* the cast is historical cruft */
               sizeof(int)); /* length of option value */
  }

  void wrapFd(int fd) {
    if (fd != -1) {
      sockFd = fd;
      int flag = 1;
      setsockopt(sockFd,       /* socket affected */
                 IPPROTO_TCP,  /* set option at TCP level */
                 TCP_NODELAY,  /* name of option */
                 (char*)&flag, /* the cast is historical cruft */
                 sizeof(int)); /* length of option value */

      isClosed = false;
    }
  }

  size_t read(uint8_t* buf, size_t len) {
    ssize_t res = recv(sockFd, (char*)buf, len, 0);
    if (res < 0) {
      isClosed = true;
      throw std::runtime_error("error in recv");
    }
    return res;
  }

  size_t write(const uint8_t* buf, size_t len) {
    ssize_t res = send(sockFd, (char*)buf, len, 0);
    if (res < 0) {
      isClosed = true;
      throw std::runtime_error("error in read");
    }
    return res;
  }

  size_t poll() {
#ifdef _WIN32
    unsigned long value;
    ioctlsocket(sockFd, FIONREAD, &value);
#else
    int value;
    ioctl(sockFd, FIONREAD, &value);
#endif
    return value;
  }

  bool isOpen() { return !isClosed; }

  void close() {
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

#endif
