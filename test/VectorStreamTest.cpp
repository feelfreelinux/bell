#include <doctest/doctest.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include "bell/io/VectorStream.h"

TEST_CASE("OVectorStream appends and reuses a byte vector") {
  std::vector<std::byte> bytes;
  bytes.reserve(8);
  bytes.push_back(std::byte{'a'});
  const auto capacity = bytes.capacity();

  bell::io::OVectorStream stream(bytes);
  stream.write("bc", 2);
  stream.put('d');
  REQUIRE(stream.good());
  CHECK(std::string_view(reinterpret_cast<const char*>(bytes.data()),
                         bytes.size()) == "abcd");

  bytes.clear();
  stream.write("xy", 2);
  REQUIRE(stream.good());
  CHECK(std::string_view(reinterpret_cast<const char*>(bytes.data()),
                         bytes.size()) == "xy");
  CHECK(bytes.capacity() == capacity);
}
