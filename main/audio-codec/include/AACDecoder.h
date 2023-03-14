#pragma once

#include "BaseCodec.h"
#include "aacdec.h"

namespace bell {

class AACDecoder : public BaseCodec {
 private:
  HAACDecoder aac;
  int16_t* pcmData;
  AACFrameInfo frame = {};

 public:
  AACDecoder();
  ~AACDecoder();
  bool setup(uint32_t sampleRate, uint8_t channelCount,
             uint8_t bitDepth) override;
  bool setup(AudioContainer* container) override;
  uint8_t* decode(uint8_t* inData, uint32_t& inLen, uint32_t& outLen) override;
};
}  // namespace bell
