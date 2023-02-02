#pragma once

#include "BaseCodec.h"
#include "mp3dec.h"

namespace bell {
class MP3Decoder : public BaseCodec {
 private:
  HMP3Decoder mp3;
  int16_t* pcmData;
  MP3FrameInfo frame = {};

 public:
  MP3Decoder();
  ~MP3Decoder();
  bool setup(uint32_t sampleRate, uint8_t channelCount,
             uint8_t bitDepth) override;
  uint8_t* decode(uint8_t* inData, uint32_t inLen, uint32_t& outLen) override;
};
}  // namespace bell
