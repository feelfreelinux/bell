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
  printf("pMemRequirement: %d\n", pMemRequirement);
  pMem = malloc(pMemRequirement);

  // Initialize the decoder buffers
  inputBuffer.resize(PVMP4AUDIODECODER_INBUFSIZE * 2);
  outputBuffer.resize(4096);

  aacDecoder->pOutputBuffer_plus = &outputBuffer[2048];
  aacDecoder->pOutputBuffer = &outputBuffer[0];
  aacDecoder->pInputBuffer = &inputBuffer[0];
  aacDecoder->inputBufferMaxLength = inputBuffer.size();

  // Settings
  aacDecoder->desiredChannels = 2;
  aacDecoder->outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
  aacDecoder->repositionFlag = TRUE;
  aacDecoder->aacPlusEnabled = TRUE;

  // State
  aacDecoder->inputBufferCurrentLength = 0;
  aacDecoder->inputBufferUsedLength = 0;
  aacDecoder->remainderBits = 0;
  aacDecoder->frameLength = 0;

  synchronized = false;

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
  synchronized = false;
  return true;
}

bool AACDecoder::setup(AudioContainer* container) {
  synchronized = false;
  return true;
}

uint8_t* AACDecoder::decode(uint8_t* inData, uint32_t& inLen,
                            uint32_t& outLen) {
  if (!inData || inLen == 0)
    return nullptr;

  aacDecoder->inputBufferCurrentLength = inLen;
  aacDecoder->inputBufferUsedLength = 0;
  aacDecoder->remainderBits = 0;
  aacDecoder->frameLength = 0;

  memcpy(aacDecoder->pInputBuffer, inData, inLen);

  int32_t status = PVMP4AudioDecodeFrame(aacDecoder, pMem);

  printf("Used length: %d\n", aacDecoder->inputBufferUsedLength);
  printf("Frame length: %d\n", aacDecoder->frameLength);
  inLen -= aacDecoder->inputBufferUsedLength;

  if (getDecodedStreamType() == -1) {
    return nullptr;
  }

  if (status != MP4AUDEC_SUCCESS) {
    printf("AAC decode error: %d\n", status);
    BELL_SLEEP_MS(100);
    return nullptr;
  }

  outLen = aacDecoder->frameLength * sizeof(int16_t);

  // Handle AAC+
  if (aacDecoder->aacPlusUpsamplingFactor == 2) {
    outLen *= 2;

    if (!synchronized) {
      printf("AAC+ detected\n");
    }
  }

  if (!synchronized) {
    int streamType = getDecodedStreamType();

    if (streamType == AAC && aacDecoder->aacPlusUpsamplingFactor == 2) {
      printf("AAC+ Disable\n");
      PVMP4AudioDecoderDisableAacPlus(aacDecoder, pMem);
      outLen = aacDecoder->frameLength * sizeof(int16_t);
    }

    printf("AAC Synchronized, %d\n", streamType);
  }

  if (!synchronized) {
    synchronized = true;
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
