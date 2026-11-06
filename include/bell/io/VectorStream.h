#pragma once

#include <cstddef>
#include <ostream>
#include <streambuf>
#include <vector>

namespace bell::io {

// Appends to a caller-owned byte vector, clearing it keeps the capacity for reuse
class VectorBuffer : public std::streambuf {
 public:
  explicit VectorBuffer(std::vector<std::byte>& buffer);

 protected:
  int_type overflow(int_type ch) override;
  std::streamsize xsputn(const char* data, std::streamsize size) override;

 private:
  std::vector<std::byte>& buffer;
};

// Growable output stream over a caller-owned byte vector
class OVectorStream : private VectorBuffer, public std::ostream {
 public:
  explicit OVectorStream(std::vector<std::byte>& buffer);
};

}  // namespace bell::io

namespace bell {
using OVectorStream = io::OVectorStream;
}  // namespace bell
