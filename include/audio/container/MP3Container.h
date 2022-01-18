// Copyright (c) Kuba Szczodrzyński 2022-1-18.

#pragma once

#include "BaseContainer.h"

struct Frame;

class MP3Container : public BaseContainer {
  public:
	~MP3Container();
	bool parse() override;
	int32_t getLoadingOffset(uint32_t timeMs) override;
	bool seekTo(uint32_t timeMs) override;
	int32_t getCurrentTimeMs() override;
	uint8_t *readSample(uint32_t &len) override;

  private:
	Frame *frame = nullptr;
	bool isCBR = true;
	uint16_t bitrate = 0;
	uint32_t frameCount = 0;
	uint32_t fileOffset = 0;
	uint32_t fileSize = 0;
	uint64_t position = 0;
	uint8_t *toc = nullptr;

  private:
	bool readFrame();
};
