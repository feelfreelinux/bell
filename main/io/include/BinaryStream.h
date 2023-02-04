#pragma once

#include <bit>
#include <iostream>
#include <vector>

namespace bell {
class BinaryStream {
 private:
  std::endian byteOrder;

  std::istream* istr = nullptr;
  std::ostream* ostr = nullptr;

  void ensureReadable();
  void ensureWritable();
  bool flipBytes = false;

  template <typename T>
  T swap16(T value) {
    return __builtin_bswap16(value);
  }
  template <typename T>
  T swap32(T value) {
    return __builtin_bswap32(value);
  }
  template <typename T>
  T swap64(T value) {
    return __builtin_bswap64(value);
  }

 public:
  BinaryStream(std::ostream* ostr);
  BinaryStream(std::istream* istr);

  /**
   * @brief Set byte order used by stream.
   *
   * @param byteOrder stream's byteorder. Defaults to native.
   */
  void setByteOrder(std::endian byteOrder);

  // Read operations
  BinaryStream& operator>>(char& value);
  BinaryStream& operator>>(std::byte& value);
  BinaryStream& operator>>(int16_t& value);
  BinaryStream& operator>>(uint16_t& value);
  BinaryStream& operator>>(int32_t& value);
  BinaryStream& operator>>(uint32_t& value);
  BinaryStream& operator>>(int64_t& value);
  BinaryStream& operator>>(uint64_t& value);

  // Write operations
  BinaryStream& operator<<(char value);
  BinaryStream& operator<<(std::byte value);
  BinaryStream& operator<<(int16_t value);
  BinaryStream& operator<<(uint16_t value);
  BinaryStream& operator<<(int32_t value);
  BinaryStream& operator<<(uint32_t value);
  BinaryStream& operator<<(int64_t value);
  BinaryStream& operator<<(uint64_t value);
};
}  // namespace bell
