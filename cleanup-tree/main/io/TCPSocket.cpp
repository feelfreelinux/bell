#include "TCPSocket.h"

#include "utils/Logger.h"

// Platform specific socket includes
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
#include <sys/types.h>
#include <unistd.h>
#ifdef __sun
#include <sys/filio.h>
#endif
#endif

using namespace bell;

TCPSocket::~TCPSocket() {
  close();
}

void TCPSocket::open(const std::string& host, uint16_t port) {
  int err;
  int domain = AF_INET;
  int socketType = SOCK_STREAM;

  struct addrinfo hints {};
  struct addrinfo* addr;

  //fine-tune hints according to which socket you want to open
  hints.ai_family = domain;
  hints.ai_socktype = socketType;
  hints.ai_protocol =
      IPPROTO_IP;  // no enum : possible value can be read in /etc/protocols
  hints.ai_flags = AI_CANONNAME | AI_ALL | AI_ADDRCONFIG;

  char portStr[6];
  snprintf(portStr, sizeof(portStr), "%u", port);
  err = getaddrinfo(host.c_str(), portStr, &hints, &addr);
  if (err != 0) {
    throw std::runtime_error("Resolve failed");
  }

  sockFd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

  if (sockFd < 0) {
    BELL_LOG(error, "http", "Could not create socket to %s, port %d. Error %d",
             host.c_str(), port, errno);
    throw std::runtime_error("Sock create failed");
  }

  isClosed = false;
  err = connect(sockFd, addr->ai_addr, addr->ai_addrlen);
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

  freeaddrinfo(addr);
  isClosed = false;
}

int TCPSocket::getFd() {
  return sockFd;
}