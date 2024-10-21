#pragma once

#include "Socket.h"

namespace bell {
/**
 * @brief TCP implementation of the bell::Socket
 */
class TCPSocket : public bell::Socket {
 public:
  TCPSocket() = default;
  ~TCPSocket() override;

  void open(const std::string& host, uint16_t port) override;
  void wrapFd(int fd) override;
  int getFd() override;
  size_t read(uint8_t* buf, size_t len) override;
  size_t write(const uint8_t* buf, size_t len) override;
  size_t poll() override;
  bool isOpen() override;
  void close() override;

 private:
  int sockFd = -1;
  bool isClosed = true;
};
}  // namespace bell