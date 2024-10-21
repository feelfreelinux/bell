#pragma once

#include <cstddef>
#include <istream>
#include <streambuf>

namespace bell {
struct MemoryBuffer : std::streambuf {
  MemoryBuffer(std::byte const* base, size_t size) : bytesWritten(0) {
    // Input buffer
    std::byte* p(const_cast<std::byte*>(base));
    this->setg((char*)p, (char*)p, (char*)p + size);
  }

  MemoryBuffer(std::byte* base, size_t size) : bytesWritten(0) {
    // Output buffer
    setp(reinterpret_cast<char*>(base), reinterpret_cast<char*>(base + size));
  }

  // Override overflow to signal that the buffer is full
  int_type overflow(int_type ch) override {
    (void)ch;
    return traits_type::eof();  // Signal failure to write
  }

  // Override sync to indicate success
  int sync() override {
    bytesWritten = pptr() - pbase();  // Update bytesWritten
    return 0;                         // Success
  }

  // Get the number of bytes written
  std::size_t getBytesWritten() const { return bytesWritten; }

 private:
  std::size_t bytesWritten;
};
struct IMemoryStream : virtual MemoryBuffer, std::istream {
  IMemoryStream(const std::byte* base, size_t size)
      : MemoryBuffer(base, size),
        std::istream(static_cast<std::streambuf*>(this)) {}
};

struct OMemoryStream : virtual MemoryBuffer, std::ostream {
  OMemoryStream(std::byte* buffer, std::size_t size)
      : MemoryBuffer(buffer, size),
        std::ostream(static_cast<std::streambuf*>(this)) {}
};
}  // namespace bell
