// Copyright (c) Kuba Szczodrzyński 2022-1-12.

#pragma once

#include "BaseCodec.h"
#include "aacdec.h"

class AACDecoder : public BaseCodec {
  private:
	HAACDecoder aac;
	short *pcmData;
	AACFrameInfo frame = {};

  public:
	AACDecoder();
	~AACDecoder();
	bool setup(uint32_t sampleRate, uint8_t channelCount, uint8_t bitWidth) override;
	uint8_t *decode(char *inData, size_t inLen, size_t &outLen) override;
};
