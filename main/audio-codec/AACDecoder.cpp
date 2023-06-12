#include "AACDecoder.h"

#include <stdlib.h>  // for free, malloc
#include <string.h>

#include "BellUtils.h"
#include "CodecType.h"  // for bell
#include "e_tmp4audioobjecttype.h"
#include "pvmp4audiodecoder_api.h"

namespace bell {
class AudioContainer;
}  // namespace bell

using namespace bell;

AACDecoder::AACDecoder() {
  aacDecoder =
      (tPVMP4AudioDecoderExternal*)malloc(sizeof(tPVMP4AudioDecoderExternal));

  int32_t pMemRequirement = PVMP4AudioDecoderGetMemRequirements();
  pMem = malloc(pMemRequirement);
  memset(aacDecoder, 0, sizeof(tPVMP4AudioDecoderExternal));
  memset(pMem, 0, pMemRequirement);

  // Initialize the decoder buffers
  outputBuffer.resize(4096);

  aacDecoder->pOutputBuffer_plus = &outputBuffer[2048];
  aacDecoder->pOutputBuffer = &outputBuffer[0];
  aacDecoder->inputBufferMaxLength = PVMP4AUDIODECODER_INBUFSIZE;

  // Settings
  aacDecoder->desiredChannels = 2;
  aacDecoder->outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
  aacDecoder->aacPlusEnabled = TRUE;

  // State
  aacDecoder->inputBufferCurrentLength = 0;
  aacDecoder->inputBufferUsedLength = 0;
  aacDecoder->remainderBits = 0;

  firstFrame = true;

  assert(PVMP4AudioDecoderInitLibrary(aacDecoder, pMem) == MP4AUDEC_SUCCESS);
}

AACDecoder::~AACDecoder() {
  PVMP4AudioDecoderResetBuffer(pMem);
  free(aacDecoder);
}

int AACDecoder::getDecodedStreamType() {
  printf("Extended audio object type: %d\n",
         aacDecoder->extendedAudioObjectType);
  switch (aacDecoder->extendedAudioObjectType) {
    case MP4AUDIO_AAC_LC:
    case MP4AUDIO_LTP:
      return AAC;
    case MP4AUDIO_SBR:
      return AACPLUS;
    case MP4AUDIO_PS:
      return ENH_AACPLUS;
    default:
      return -1;
  }
}

bool AACDecoder::setup(uint32_t sampleRate, uint8_t channelCount,
                       uint8_t bitDepth) {
  firstFrame = true;
  return true;
}

bool AACDecoder::setup(AudioContainer* container) {
  firstFrame = true;
  return true;
}

uint8_t* AACDecoder::decode(uint8_t* inData, uint32_t& inLen,
                            uint32_t& outLen) {
  if (!inData || inLen == 0)
    return nullptr;

  aacDecoder->inputBufferCurrentLength = inLen;
  aacDecoder->inputBufferUsedLength = 0;
  aacDecoder->inputBufferMaxLength = inLen;
  aacDecoder->pInputBuffer = inData;
  aacDecoder->remainderBits = 0;
  aacDecoder->repositionFlag = false;

  int32_t status;

  if (firstFrame) {
    if (PVMP4AudioDecoderConfig(aacDecoder, pMem) != MP4AUDEC_SUCCESS) {
      status = PVMP4AudioDecodeFrame(aacDecoder, pMem);
      if (status == MP4AUDEC_SUCCESS) {
        firstFrame = false;
        int streamType = getDecodedStreamType();
        printf("Stream type: %d\n", streamType);

        if (streamType == AAC && aacDecoder->aacPlusUpsamplingFactor == 2) {
          PVMP4AudioDecoderDisableAacPlus(aacDecoder, pMem);
          printf("AAC+ disabled\n");
          outLen = aacDecoder->frameLength * sizeof(int16_t);
        }
      }
    } else {
      inLen = 0;
      return nullptr;
    }
  } else {
    status = PVMP4AudioDecodeFrame(aacDecoder, pMem);
  }

  if (status != MP4AUDEC_SUCCESS) {
    outLen = 0;
    inLen = 0;
    return nullptr;
  } else {
    inLen -= aacDecoder->inputBufferUsedLength;
  }

  outLen = aacDecoder->frameLength * sizeof(int16_t);

  // Handle AAC+
  if (aacDecoder->aacPlusUpsamplingFactor == 2) {
    outLen *= 2;
  }

  outLen *= aacDecoder->desiredChannels;
  return (uint8_t*)&outputBuffer[0];
  /*


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
  */
}
