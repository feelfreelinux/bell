#pragma once

#include <vector>
#include "BaseContainer.h"
#include "aacdec.h"

namespace bell {
class AACContainer : public BaseContainer {
 public:
  ~AACContainer();
  AACContainer();
  bool parse() override;
  int32_t getLoadingOffset(uint32_t timeMs) override;
  bool seekTo(uint32_t timeMs) override;
  int32_t getCurrentTimeMs() override;
  uint8_t* readSample(uint32_t& len) override;
  uint8_t* getSetupData(uint32_t& len, AudioCodec matchCodec) override;
  void feed(const std::shared_ptr<bell::ByteStream>& stream,
            uint32_t position) override;
private:
  static constexpr auto AAC_MAX_FRAME_SIZE = 2100;
  static constexpr auto BUFFER_SIZE = 1024 * 10;
  std::vector<uint8_t> buffer = std::vector<uint8_t>(BUFFER_SIZE);
  size_t bytesInBuffer = 0;
  bool fillBuffer();
  size_t dataOffset = 0;

};
}  // namespace bell
