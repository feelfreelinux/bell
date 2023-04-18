#include "AACDecoder.h"

#include <stdlib.h>  // for free, malloc

#include "CodecType.h"  // for bell

namespace bell {
class AudioContainer;
}  // namespace bell

using namespace bell;

AACDecoder::AACDecoder() {
  aac = AACInitDecoder();
  pcmData = (int16_t*)malloc(AAC_MAX_NSAMPS * AAC_MAX_NCHANS * sizeof(int16_t));
}

AACDecoder::~AACDecoder() {
  AACFreeDecoder(aac);
  free(pcmData);
}

bool AACDecoder::setup(uint32_t sampleRate, uint8_t channelCount,
                       uint8_t bitDepth) {
  return true;
}

bool AACDecoder::setup(AudioContainer* container) {
  return true;
}

uint8_t* AACDecoder::decode(uint8_t* inData, uint32_t& inLen,
                            uint32_t& outLen) {
  if (!inData)
    return nullptr;

  int status = AACDecode(aac, static_cast<unsigned char**>(&inData),
                         reinterpret_cast<int*>(&inLen),
                         static_cast<short*>(this->pcmData));

  AACGetLastFrameInfo(aac, &frame);
  if (status != ERR_AAC_NONE) {
    lastErrno = status;
    return nullptr;
  }

  if (sampleRate != frame.sampRateOut) {
    this->sampleRate = frame.sampRateOut;
  }

  if (channelCount != frame.nChans) {
    this->channelCount = frame.nChans;
  }

  outLen = frame.outputSamps * sizeof(int16_t);
  return (uint8_t*)pcmData;
}
