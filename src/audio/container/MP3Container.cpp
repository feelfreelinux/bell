// Copyright (c) Kuba Szczodrzyński 2022-1-18.

#include "MP3Container.h"
#include "AudioCodecs.h"
#include "BellUtils.h"
#include "MP3Types.h"
#include "mp3common.h"
#include <cmath>

// TODO add define guard for including mp3common.h

using namespace bell;

/*
 * This container was built mostly based on info from this website.
 * http://www.goat.cz/index.php?path=MP3_MP3ProfiInfo
 */

MP3Container::~MP3Container() {
	if (frame)
		free(frame->data);
	free(frame);
}

bool MP3Container::parse() {
	// allocate memory for the frame data
	freeAndNull((void *&)frame);
	frame = static_cast<Frame *>(malloc(sizeof(Frame)));
	frame->maxSize = 0;
	frame->data = static_cast<uint8_t *>(malloc(4));

	// try to read the first valid frame
	while (!readFrame()) {
		if (strncmp((char *)frame->data, "ID3", 3) == 0) {
			skipBytes(3); // skip version and flags
			// read the size
			uint32_t tagSize = 0;
			for (uint8_t i = 0; i < 3; i++) {
				tagSize <<= 7;
				tagSize |= readUint8() & 0x7F;
			}
			// skip the entire tag
			skipBytes(tagSize);
		}
	}

	char *data = (char *)frame->data;
	uint8_t infoPos = 0;
	if (frame->version == MpegVersion::MPEG_1 && frame->channels != MpegChannelMode::MONO)
		// MPEG1 not-Mono
		infoPos = 36;
	else if (frame->version == MpegVersion::MPEG_1 || frame->channels != MpegChannelMode::MONO)
		// MPEG1 Mono || MPEG2 not-Mono
		infoPos = 21;
	else if (frame->version == MpegVersion::MPEG_2 && frame->channels == MpegChannelMode::MONO)
		// MPEG2 Mono
		infoPos = 13;
	// try to find VBR Xing or CBR Info header
	if (strncmp(data + infoPos, "Xing", 4) == 0 || strncmp(data + infoPos, "Info", 4) == 0) {
		isCBR = data[infoPos] == 'I';
		uint8_t *info = frame->data + infoPos + 4;
		uint8_t flags = info[3];
		info += 4;
		if (flags & 0x01) { // Frames Flag
			frameCount = (info[0] << 24) | (info[1] << 16) | (info[2] << 8) | info[3];
			info += 4;
		}
		if (flags & 0x02) { // Bytes Flag
			fileSize = (info[0] << 24) | (info[1] << 16) | (info[2] << 8) | info[3];
			fileOffset = pos - frame->size - 4; // size preceding the Info tag, i.e. ID3
			info += 4;
		}
		if (flags & 0x04) { // TOC Flag
			toc = static_cast<uint8_t *>(malloc(100));
			memcpy(toc, info, 100);
		}
		// read the next frame, which should be audio
		if (!readFrame())
			return false;
	}

	// use the first frame's parameters as default
	sampleRate = frame->sampleRate;
	channelCount = frame->channels == MpegChannelMode::MONO ? 1 : 2;
	if (isCBR) {
		bitrate = frame->bitrate;
	}

	isParsed = true;
	isSeekable = isCBR || toc;
	codec = AudioCodec::MP3;
	return true;
}

int32_t MP3Container::getLoadingOffset(uint32_t timeMs) {
	return 0;
}

bool MP3Container::seekTo(uint32_t timeMs) {
	return false;
}

int32_t MP3Container::getCurrentTimeMs() {
	return (int32_t)(position * 1000 / sampleRate);
}

uint8_t *MP3Container::readSample(uint32_t &len) {
	// read a new frame if nothing loaded, return on failure
	if (!isParsed || (!frame->size && !readFrame()))
		return nullptr;
	len = frame->size;
	frame->size = 0;
	return frame->data;
}

uint16_t bitrateFromIndex(uint8_t i) {
	i -= 1;
	auto p = (uint8_t)pow(2, (uint8_t)(i / 4));
	return 32 * p + 8 * p * (i % 4);
}

bool MP3Container::readFrame() {
	auto *header = frame->data;
	readBytes(frame->data, 4);
	if (header[0] != 0xFF || (header[1] & 0xE0) != 0xE0)
		return false;
	frame->version = (MpegVersion)(3 - (header[1] >> 3 & 0b11));
	if (frame->version == MpegVersion::RESERVED)
		frame->version = MpegVersion::MPEG_2_5;
	frame->layer = 4 - (header[1] >> 1 & 0b11);
	frame->hasCrc = header[1] & 1;
	frame->bitrate = bitrateFromIndex(header[2] >> 4);
	frame->sampleRate = header[2] >> 2 & 0b11;
	frame->sampleRate = frame->sampleRate ? 32000 + 16000 * (2 - frame->sampleRate) : 44100;
	frame->hasPadding = header[2] >> 1 & 1;
	frame->channels = (MpegChannelMode)(header[3] >> 6);
	frame->size = (144 * frame->bitrate * 1000 / frame->sampleRate) + frame->hasPadding;
	if (frame->size > frame->maxSize) {
		frame->data = static_cast<uint8_t *>(realloc(frame->data, frame->size));
		frame->maxSize = frame->size;
	}
	readBytes(frame->data + 4, frame->size - 4);
	if (isCBR && bitrate != frame->bitrate) {
		isCBR = false;
	}
	position += samplesPerFrameTab[static_cast<uint8_t>(frame->version)][frame->layer - 1];
	return true;
}
