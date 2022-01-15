// Copyright (c) Kuba Szczodrzyński 2022-1-12.

#include "AACDecoder.h"

AACDecoder::AACDecoder() {
	aac = AACInitDecoder();
	pcmData = (short *)malloc(AAC_MAX_NSAMPS * AAC_MAX_NCHANS * sizeof(short));
}

AACDecoder::~AACDecoder() {
	AACFreeDecoder(aac);
	free(pcmData);
}

bool AACDecoder::setup(uint32_t sampleRate, uint8_t channelCount, uint8_t bitWidth) {
	frame.sampRateCore = (int)sampleRate;
	frame.nChans = channelCount;
	frame.bitsPerSample = bitWidth;
	return AACSetRawBlockParams(aac, 0, &frame) == 0;
}

uint8_t *AACDecoder::decode(char *inData, size_t inLen, size_t &outLen) {
	if (!inData)
		return nullptr;
	int status = AACDecode(aac, (unsigned char **)&inData, (int *)&inLen, this->pcmData);
	AACGetLastFrameInfo(aac, &frame);
	if (status != ERR_AAC_NONE) {
		lastErrno = status;
		return nullptr;
	}
	outLen = frame.outputSamps * sizeof(short);
	return (uint8_t *)pcmData;
}
