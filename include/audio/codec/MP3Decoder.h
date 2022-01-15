// Copyright (c) Kuba Szczodrzyński 2022-1-14.

#pragma once

#include "BaseCodec.h"
#include "mp3dec.h"

class MP3Decoder : public BaseCodec {
  private:
	HMP3Decoder mp3;
	short *pcmData;
	MP3FrameInfo frame = {};

  public:
	MP3Decoder();
	~MP3Decoder();
	bool setup(uint32_t sampleRate, uint8_t channelCount, uint8_t bitDepth) override;
	uint8_t *decode(char *inData, size_t inLen, size_t &outLen) override;
};
