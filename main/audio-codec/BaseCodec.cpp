#include "BaseCodec.h"

using namespace bell;

bool BaseCodec::setup(AudioContainer* container) {
  return false;
}

uint8_t* BaseCodec::decode(AudioContainer* container, uint32_t& outLen) {
  uint32_t len;
  auto* data = container->readSample(len);
  return decode((uint8_t*) data, len, outLen);
}
