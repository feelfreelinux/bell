#include <catch2/catch.hpp>
#include "BinaryStream.h"

#include <sstream>        // for std::ostringstream
#include "StreamUtils.h"  // for bell::IMemoryStream

TEST_CASE("BinaryStream encodes and decodes data properly", "[BinaryStream]") {
  // Static so we retain value through the test
  static std::ostringstream result;
  static int idx = 0;

  auto bs = bell::BinaryStream(&result);

  // Request big endian
  bs.setByteOrder(std::endian::big);

  // Sample values
  std::byte byteVal{10};
  char chVal = 'a';
  int16_t int16Val = -2137;
  uint16_t uint16Val = 2137;
  int32_t int32Val = -3333;
  uint32_t uint32Val = 44224;
  int64_t int64Val = -22152211245125;
  uint64_t uint64Val = 123123123123;

  SECTION("throws when trying to read writable stream") {
    char testVal;

    REQUIRE_THROWS(bs >> testVal);
  }

  SECTION("encodes char and byte properly") {
    bs << byteVal;
    bs << chVal;

    // No swap at 1 byte value
    REQUIRE((std::byte)result.str()[idx++] == byteVal);
    REQUIRE(result.str()[idx++] == chVal);
  }

  SECTION("encodes 16bit values") {
    bs << int16Val;
    bs << uint16Val;

    auto res = result.str();

    // Test signed swap
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int16Val >> 8) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == (int16Val & 0xFF));

    // Test unsigned swap
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint16Val >> 8) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == (uint16Val & 0xFF));
  }

  SECTION("encodes 32bit values") {
    bs << int32Val;
    bs << uint32Val;

    auto res = result.str();

    // Test signed swap
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int32Val >> 24) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int32Val >> 16) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int32Val >> 8) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == (int32Val & 0xFF));

    // Test unsigned swap
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint32Val >> 24) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint32Val >> 16) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint32Val >> 8) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == (uint32Val & 0xFF));
  }

  SECTION("encodes 64bit values") {
    bs << int64Val;
    bs << uint64Val;

    auto res = result.str();

    // Test signed swap
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int64Val >> 56) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int64Val >> 48) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int64Val >> 40) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int64Val >> 32) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int64Val >> 24) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int64Val >> 16) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((int64Val >> 8) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == (int64Val & 0xFF));

    // Test unsigned swap
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint64Val >> 56) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint64Val >> 48) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint64Val >> 40) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint64Val >> 32) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint64Val >> 24) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint64Val >> 16) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == ((uint64Val >> 8) & 0xFF));
    REQUIRE(static_cast<uint8_t>(res[idx++]) == (uint64Val & 0xFF));
  }

  SECTION("decodes values properly") {
    // Copy previously encoded data into a string, wrap with memorystream
    std::string res = result.str();
    bell::IMemoryStream istr((std::byte*)res.data(), res.size());

    // Create binary stream wrapping the previously encoded data
    bs = bell::BinaryStream(&istr);
    bs.setByteOrder(std::endian::big);
    auto resVector = std::vector<uint8_t>(res.begin(), res.end());

    // Test byte parsing
    std::byte readByteVal;
    char readCharVal;
    bs >> readByteVal;
    bs >> readCharVal;
    REQUIRE(readByteVal == byteVal);
    REQUIRE(readCharVal == chVal);

    // Test 16bit ints
    int16_t readInt16Val;
    uint16_t readUint16Val;
    bs >> readInt16Val;
    bs >> readUint16Val;
    REQUIRE(readInt16Val == int16Val);
    REQUIRE(readUint16Val == uint16Val);

    // Test 32bit ints
    int32_t readInt32Val;
    uint32_t readUint32Val;
    bs >> readInt32Val;
    bs >> readUint32Val;
    REQUIRE(readInt32Val == int32Val);
    REQUIRE(readUint32Val == uint32Val);

    // Test 64bit ints
    int64_t readInt64Val;
    uint64_t readUint64Val;
    bs >> readInt64Val;
    bs >> readUint64Val;
    REQUIRE(readInt64Val == int64Val);
    REQUIRE(readUint64Val == uint64Val);
  }
}