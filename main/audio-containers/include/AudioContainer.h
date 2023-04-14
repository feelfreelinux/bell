#pragma once

#include <cstddef>
#include <cstring>
#include <istream>
#include "CodecType.h"
#include "StreamInfo.h"

namespace bell {
class AudioContainer {
 protected:
  std::istream& istr;
  uint32_t toConsume = 0;

 public:
  bell::SampleRate sampleRate;
  bell::BitWidth bitWidth;
  int channels;

  AudioContainer(std::istream& istr) : istr(istr) {}

  virtual std::byte* readSample(uint32_t& len) = 0;
  void consumeBytes(uint32_t bytes) { this->toConsume = bytes; }
  virtual void parseSetupData() = 0;
  virtual bell::AudioCodec getCodec() = 0;
};
}  // namespace bell
