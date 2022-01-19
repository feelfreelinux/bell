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

void MP3Container::feed(const std::shared_ptr<bell::ByteStream> &stream, uint32_t position) {
	BaseContainer::feed(stream, position);
	findFrame(false); // find the nearest sync word
	setTimeFromPos();
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
		fileOffset = pos; // skip all invalid frames
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
		// don't count the info frame to audio position
		currentTime = 0;
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
	// estimate the total track duration
	if (frameCount) {
		durationMs = frameCount * frame->sampleCount * 1000LL / frame->sampleRate;
	} else if (isCBR && source->size()) {
		durationMs = source->size() / frame->size * frame->sampleCount * 1000LL / frame->sampleRate;
	}

	isParsed = true;
	isSeekable = isCBR || toc;
	codec = AudioCodec::MP3;
	return true;
}

int32_t MP3Container::getLoadingOffset(uint32_t timeMs) {
	if (!isParsed)
		return SAMPLE_NOT_LOADED;
	if (!isCBR && !(toc && fileSize && frameCount))
		return SAMPLE_NOT_SEEKABLE;

	if (isCBR) {
		auto sampleNum = (uint32_t)((float)frame->sampleRate / 1000.0f * (float)timeMs);
		uint32_t frameNum = sampleNum / frame->sampleCount;
		return (int32_t)(fileOffset + frameNum * frame->size);
	}

	// VBR has durationMs calculated in parse() (frameCount is set)
	auto i = (uint8_t)((float)timeMs * 100.0f / (float)durationMs);
	if (i >= 100)
		return SAMPLE_NOT_FOUND;
	auto offset = (float)toc[i] * (float)fileSize / 256.0f;
	return (int32_t)fileOffset + (int32_t)offset;
}

bool MP3Container::seekTo(uint32_t timeMs) {
	if (!isParsed || !frame)
		return false;
	auto sampleNum = (uint32_t)((float)frame->sampleRate / 1000.0f * (float)timeMs);
	if (sampleNum <= currentTime)
		return true; // already at this position
	while (!closed && sampleNum > currentTime) {
		readFrame(); // read all frames until the requested position
	}
	findFrame(true); // find the nearest independent frame
	return !closed;
}

int32_t MP3Container::getCurrentTimeMs() {
	return (int32_t)(currentTime * 1000 / sampleRate);
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

bool MP3Container::readFrame(bool noRead) {
	auto *header = frame->data;
	if (!noRead)
		readBytes(frame->data, 4);
	if (closed || header[0] != 0xFF || (header[1] & 0xE0) != 0xE0)
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
	frame->sampleCount = samplesPerFrameTab[static_cast<uint8_t>(frame->version)][frame->layer - 1];
	frame->size = (144 * frame->bitrate * 1000 / frame->sampleRate) + frame->hasPadding;
	if (frame->size > frame->maxSize) {
		frame->data = static_cast<uint8_t *>(realloc(frame->data, frame->size));
		frame->maxSize = frame->size;
	}
	readBytes(frame->data + 4, frame->size - 4);
	if (isCBR && bitrate && bitrate != frame->bitrate) {
		isCBR = false;
	}
	currentTime += frame->sampleCount;
	return true;
}

bool MP3Container::findFrame(bool independentOnly) {
	if (!frame)
		return false;
	do {
		if (closed)
			return false;
		frame->data[0] = readUint8();
		// check for 11-bit sync word and Layer to be not 0b00
		if (frame->data[0] != 0xFF)
			continue;
		frame->data[1] = readUint8();
		if ((frame->data[1] & 0xE0) != 0xE0 || (frame->data[1] & 0x06) == 0x00)
			continue;
		frame->data[2] = readUint8();
		frame->data[3] = readUint8();
		readFrame(true);
		// what
		// frame->data[4] = 0x00;
		// frame->data[5] &= 0x7F;
		// check if this frame is independent
		/*if (independentOnly && frame->layer == 3) {
			// skip any frames using the bit reservoir
			if (frame->data[4] != 0x00 || frame->data[5] >> 7 != 0b0)
				continue;
		}*/
		break;
	} while (!closed);
	return !closed;
}

void MP3Container::setTimeFromPos() {
	if (!frame || !frame->size || pos <= fileOffset)
		return;
	if (isCBR) {
		uint32_t frameNum = pos / frame->size;
		currentTime = frameNum * frame->sampleCount;
		return;
	}
	if (!toc || !fileSize || !frameCount)
		return;
	auto tocByte = (uint8_t)((float)pos / (float)fileSize * 256.0f);
	for (uint8_t i = 0; i < 100; i++) {
		if (tocByte <= toc[i]) {
			currentTime = i * frameCount * frame->sampleCount / 100L;
			return;
		}
	}
}
