// Copyright (c) Kuba Szczodrzyński 2022-1-18.

#pragma once

#include <cstdint>

/* full MP3 header definition */
/*typedef struct {
	uint8_t sync1 : 8;
	bool crc : 1;
	uint8_t layer : 2;
	uint8_t version : 2;
	uint8_t sync2 : 3;
	bool privBit : 1;
	bool padding : 1;
	uint8_t freqIndex : 2;
	uint8_t bitrateIndex : 4;
	uint8_t emphasis : 2;
	bool original : 1;
	bool copyright : 1;
	uint8_t modeExtension : 2;
	uint8_t channelMode : 2;
} Header;*/

enum class MpegVersion {
	MPEG_1 = 0,
	MPEG_2 = 1,
	MPEG_2_5 = 2,
	RESERVED = 3,
};

enum class MpegChannelMode {
	STEREO = 0b00,
	JOINT_STEREO = 0b01,
	DUAL_MONO = 0b10,
	MONO = 0b11,
};

typedef struct Frame {
	MpegVersion version;
	uint8_t layer;
	bool hasCrc;
	uint16_t bitrate;
	uint16_t sampleRate;
	bool hasPadding;
	MpegChannelMode channels;
	uint16_t size;
	uint16_t maxSize;
	uint8_t *data;
} Frame;
