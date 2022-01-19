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
	void feed(const std::shared_ptr<bell::ByteStream> &stream, uint32_t position) override;

  private:
	Frame *frame = nullptr;
	bool isCBR = true;
	uint16_t bitrate = 0;	  // previous frame bitrate
	uint32_t frameCount = 0;  // total frame count in the file, only from Info tag
	uint32_t fileOffset = 0;  // offset of first MP3 frame in the file (either Info or audio)
	uint32_t fileSize = 0;	  // total file size, starting at fileOffset
	uint64_t currentTime = 0; // current playing time, in samples
	uint8_t *toc = nullptr;

  private:
	bool readFrame(bool noRead = false); // noRead is used for syncing to next frame
	void setTimeFromPos();
	bool findFrame(bool independentOnly);
};
