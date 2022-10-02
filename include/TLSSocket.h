#ifndef BELL_TLS_SOCKET_H
#define BELL_TLS_SOCKET_H

#include "BellLogger.h"
#include "BellSocket.h"
#include <cstring>
#include <ctype.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"

namespace bell {
class TLSSocket : public bell::Socket {
private:
    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;

  bool isClosed = false;
public:
  TLSSocket();
  ~TLSSocket() { close(); };

  void open(std::string host, uint16_t port);

  size_t read(uint8_t *buf, size_t len);
  size_t write(uint8_t *buf, size_t len);
  size_t poll();

  void close();
};

} // namespace bell

#endif
