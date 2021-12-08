#include "platform/TLSSocket.h"

/**
 * Platform TLSSocket implementation for the openssl
 */

bell::TLSSocket::TLSSocket() {
  ERR_load_crypto_strings();
  ERR_load_SSL_strings();
  OpenSSL_add_all_algorithms();
  ctx = SSL_CTX_new(SSLv23_client_method());
}

void bell::TLSSocket::open(std::string url) {

  /* We'd normally set some stuff like the verify paths and
   * mode here because as things stand this will connect to
   * any server whose certificate is signed by any CA.
   */

  sbio = BIO_new_ssl_connect(ctx);

  BIO_get_ssl(sbio, &ssl);
  SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

  url.erase(0, url.find("://") + 3);
  std::string hostUrl = url.substr(0, url.find('/'));
  std::string pathUrl = url.substr(url.find('/'));

  std::string portString = "443";
  // check if hostUrl contains ':'
  if (hostUrl.find(':') != std::string::npos) {
    // split by ':'
    std::string host = hostUrl.substr(0, hostUrl.find(':'));
    portString = hostUrl.substr(hostUrl.find(':') + 1);
    hostUrl = host;
  }

  BELL_LOG(info, "http_tls", "Connecting with %s", hostUrl.c_str());
  BIO_set_conn_hostname(sbio, std::string(hostUrl + ":443").c_str());

  out = BIO_new_fp(stdout, BIO_NOCLOSE);
  if (BIO_do_connect(sbio) <= 0) {
    BELL_LOG(error, "http_tls", "Error connecting with server");
    /* whatever ... */
  }

  if (BIO_do_handshake(sbio) <= 0) {

    BELL_LOG(error, "http_tls", "Error TLS connection");

    /* whatever ... */
  } // remove https or http from url

  // split by first "/" in url
}

size_t bell::TLSSocket::read(uint8_t *buf, size_t len) {
  return BIO_read(sbio, buf, len);
}

size_t bell::TLSSocket::write(uint8_t *buf, size_t len) {
  return BIO_write(sbio, buf, len);
}

void bell::TLSSocket::close() {
  if (!isClosed) {
    BIO_free_all(sbio);
    BIO_free(out);
    isClosed = true;
  }
}
