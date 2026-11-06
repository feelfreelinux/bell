#include "bell/io/VectorStream.h"

namespace bell::io {

VectorBuffer::VectorBuffer(std::vector<std::byte>& buffer) : buffer(buffer) {}

VectorBuffer::int_type VectorBuffer::overflow(int_type ch) {
  if (traits_type::eq_int_type(ch, traits_type::eof()))
    return traits_type::not_eof(ch);
  buffer.push_back(static_cast<std::byte>(traits_type::to_char_type(ch)));
  return ch;
}

std::streamsize VectorBuffer::xsputn(const char* data, std::streamsize size) {
  if (size <= 0)
    return 0;
  const auto* first = reinterpret_cast<const std::byte*>(data);
  buffer.insert(buffer.end(), first, first + size);
  return size;
}

OVectorStream::OVectorStream(std::vector<std::byte>& buffer)
    : VectorBuffer(buffer), std::ostream(static_cast<std::streambuf*>(this)) {}

}  // namespace bell::io
