#pragma once

#include <stdint.h>  // for uint32_t
#include <cstddef>   // for byte, size_t
#include <istream>   // for istream
#include <vector>    // for vector

#include "AudioContainer.h"  // for AudioContainer
#include "CodecType.h"       // for AudioCodec, AudioCodec::AAC

namespace bell {
class AACContainer : public AudioContainer {
 public:
  ~AACContainer(){};
  AACContainer(std::istream& istr);

  std::byte* readSample(uint32_t& len) override;
  void parseSetupData() override;

  bell::AudioCodec getCodec() override { return bell::AudioCodec::AAC; }

 private:
  static constexpr auto AAC_MAX_FRAME_SIZE = 2100;
  static constexpr auto BUFFER_SIZE = 1024 * 10;

  std::vector<std::byte> buffer = std::vector<std::byte>(BUFFER_SIZE);

  size_t bytesInBuffer = 0;
  size_t dataOffset = 0;

  bool fillBuffer();
};
}  // namespace bell
