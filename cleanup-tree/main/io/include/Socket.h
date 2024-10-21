#pragma once

#include <string>

// For addrinfo
#include <netdb.h>

namespace bell {
/**
 * @brief Base pure socket class to be implemented by different socket types.
 * 
 * This class provides a standard interface for socket operations, which can be 
 * extended by different socket types (e.g., TCP, UDP). It defines essential 
 * methods for opening, closing, reading from, writing to, and polling a socket, 
 * as well as wrapping existing file descriptors.
 */
class Socket {
 public:
  Socket() = default;  ///< Default constructor.
  virtual ~Socket() =
      default;  ///< Virtual destructor for proper cleanup in derived classes.

  /**
   * @brief Resolve the provided host and port, and attempt to create a socket connected there.
   * 
   * This method resolves the hostname and attempts to connect to the specified port.
   * It must be implemented by derived classes according to the specific socket type.
   *
   * @param host String containing a hostname or IP address to connect to.
   * @param port The port number to connect to on the specified host.
   */
  virtual void open(const std::string& host, uint16_t port) = 0;

  /**
   * @brief Wrap an existing file descriptor with this socket.
   * 
   * This method allows an existing file descriptor (fd) to be wrapped and 
   * treated as a socket within the application. This can be useful for integrating
   * sockets created by other means or from external sources.
   *
   * @param fd File descriptor to wrap.
   */
  virtual void wrapFd(int fd) = 0;

  /**
   * @brief Poll the socket for events.
   * 
   * This method checks the socket for any events (e.g., data available for reading, 
   * socket ready for writing). The return value indicates the status of the poll 
   * operation, which can be used to determine the next actions on the socket.
   *
   * @return The number of events detected. A return value of 0 indicates no events, 
   * while a positive number indicates the number of events detected.
   */
  virtual size_t poll() = 0;

  /**
   * @brief Write data to the socket.
   * 
   * This method sends data from the provided buffer to the socket. The method 
   * blocks until the data is sent or an error occurs. The return value indicates 
   * the number of bytes successfully written.
   *
   * @param buf Pointer to the buffer containing the data to send.
   * @param len The number of bytes to write from the buffer.
   * @return The number of bytes successfully written.
   */
  virtual size_t write(const uint8_t* buf, size_t len) = 0;

  /**
   * @brief Read data from the socket.
   * 
   * This method receives data from the socket and stores it in the provided buffer. 
   * The method blocks until data is available or an error occurs. The return value 
   * indicates the number of bytes successfully read.
   *
   * @param buf Pointer to the buffer where the received data will be stored.
   * @param len The maximum number of bytes to read into the buffer.
   * @return The number of bytes successfully read. A return value of 0 may indicate 
   * that the connection was closed, while a value less than len could indicate that 
   * no more data is currently available.
   */
  virtual size_t read(uint8_t* buf, size_t len) = 0;

  /**
   * @brief Check if the socket is currently open.
   *
   * @return True if the socket is open, false otherwise.
   */
  virtual bool isOpen() = 0;

  /**
   * @brief Close the socket.
   * 
   * This method closes the socket and releases any resources associated with it. 
   * After calling this method, the socket is no longer usable until reopened.
   */
  virtual void close() = 0;

  /**
   * @brief Get the file descriptor associated with the socket.
   *
   * @return The file descriptor associated with the socket.
   */
  virtual int getFd() = 0;

 protected:
  /**
   * @brief Helper function, that resolves a hostname into an address structure. In case the provided hostname is an ip address, it will simply parse it.
   * 
   * @param hostname domain to resolve, or a valid ipv4 or ipv6 address
   * @param targetAddr target address structure
   */
  static void prepareAddrInfo(const std::string& hostname,
                              struct addrinfo* targetAddr);
};
}  // namespace bell
