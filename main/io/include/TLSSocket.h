#ifndef BELL_TLS_SOCKET_H
#define BELL_TLS_SOCKET_H

#include <ctype.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include "BellLogger.h"
#include "BellSocket.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sstream>
#include <string>
#include <vector>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

namespace bell {
class TLSSocket : public bell::Socket {
 private:
  mbedtls_net_context server_fd;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;

  bool isClosed = true;

 public:
  TLSSocket();
  ~TLSSocket() { close(); };

  void open(const std::string& host, uint16_t port);

  size_t read(uint8_t* buf, size_t len);
  size_t write(uint8_t* buf, size_t len);
  size_t poll();
  bool isOpen();

  void close();
};

}  // namespace bell

#endif
