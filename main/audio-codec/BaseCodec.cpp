#include "BaseCodec.h"

using namespace bell;

bool BaseCodec::setup(BaseContainer* container) {
  return this->setup(container->sampleRate, container->channelCount,
                     container->bitDepth);
}

uint8_t* BaseCodec::decode(BaseContainer* container, uint32_t& outLen) {
  uint32_t len;
  auto* data = container->readSample(len);
  return decode(data, len, outLen);
}
