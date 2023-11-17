#pragma once

#include <ByteStream.h>  // for ByteStream
#include <stdint.h>      // for uint8_t
#include <stdio.h>       // for size_t, FILE
#include <string>        // for string

/**
 * @brief A class for reading and writing to files implementing the ByteStream interface.
 */
namespace bell {
class FileStream final : public ByteStream {
 public:
  FileStream(const std::string& path, std::string mode);
  ~FileStream();

  FILE* file;

  /**
   * @brief Reads data from the stream.
   *
   * @param buf The buffer to read data into.
   * @param nbytes The size of the buffer.
   * @return The number of bytes read.
   * @throws std::runtime_error if the stream is closed. 
   */
  size_t read(uint8_t* buf, size_t nbytes) override;

  /**
   * @brief Skips bytes in stream
   */
  size_t skip(size_t nbytes) override;

  size_t position() override;

  size_t size() override;

  // Closes the connection
  void close() override;
};
}  // namespace bell
