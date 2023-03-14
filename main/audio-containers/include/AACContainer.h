#pragma once

#include <cstring>
#include <cstddef>
#include <vector>
#include "AudioContainer.h"
#include "aacdec.h"

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
