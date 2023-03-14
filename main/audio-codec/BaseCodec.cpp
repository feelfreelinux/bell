#include "BaseCodec.h"
#include <iostream>

using namespace bell;

bool BaseCodec::setup(AudioContainer* container) {
  return false;
}

uint8_t* BaseCodec::decode(AudioContainer* container, uint32_t& outLen) {
  auto* data = container->readSample(lastSampleLen);
  if (data == nullptr) {
    outLen = 0;
    return nullptr;
  }

  if (lastSampleLen == 0) {
    outLen = 0;
    return nullptr;
  }

  availableBytes = lastSampleLen;
  auto* result = decode((uint8_t*)data, availableBytes, outLen);
  if (result == nullptr) {
    container->consumeBytes(1);
  } else {
    container->consumeBytes(lastSampleLen - availableBytes);
  }
  return result;
}
