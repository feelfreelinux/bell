// Copyright (c) Kuba Szczodrzyński 2022-1-14.

#pragma once

#include "BaseCodec.h"
#include "ivorbiscodec.h"

class VorbisDecoder : public BaseCodec {
  private:
	vorbis_info *vi = nullptr;
	vorbis_comment *vc = nullptr;
	vorbis_dsp_state *vd = nullptr;
	ogg_packet op = {};
	int16_t *pcmData;

  public:
	VorbisDecoder();
	~VorbisDecoder();
	bool setup(uint32_t sampleRate, uint8_t channelCount, uint8_t bitWidth) override;
	uint8_t *decode(char *inData, size_t inLen, size_t &outLen) override;
	bool setup(BaseContainer *container) override;
	void setPacket(const char *inData, size_t inLen) const;
};
