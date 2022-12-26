#include "BellDSP.h"
#include <iostream>
#include "CentralAudioBuffer.h"

using namespace bell;

class FadeOutEffect : public bell::BellDSP::AudioEffect {
public:
  FadeOutEffect() {
    this->duration = 44100;
  }
  ~FadeOutEffect() {}

  void apply(float* audioData, size_t samples, size_t relativePosition) override {
    float effect = relativePosition / (float) this->duration;
    for (int x = 0; x < samples; x++) {
      audioData[x] *= effect;
    }
  }
};

BellDSP::BellDSP(std::shared_ptr<CentralAudioBuffer> buffer) {
  this->buffer = buffer;
  this->underflowEffect = std::make_unique<FadeOutEffect>();
};

void BellDSP::applyPipeline(std::shared_ptr<AudioPipeline> pipeline) {
  std::scoped_lock lock(accessMutex);
  activePipeline = pipeline;
}

size_t BellDSP::process(uint8_t* data, size_t bytes, int channels,
                        SampleRate sampleRate, BitWidth bitWidth) {
  if (bytes > 1024 * 2 * channels) {
    throw std::runtime_error("Too many bytes");
  }

  size_t bytesPerSample = channels * 2;

  size_t samplesLeftInBuffer = buffer->audioBuffer->size() / bytesPerSample;

  // Create a StreamInfo object to pass to the pipeline
  auto streamInfo = std::make_unique<StreamInfo>();
  streamInfo->numChannels = channels;
  streamInfo->sampleRate = sampleRate;
  streamInfo->bitwidth = bitWidth;
  streamInfo->numSamples = bytes / channels / 2;

  std::scoped_lock lock(accessMutex);

  int16_t* data16Bit = (int16_t*)data;

  int length16 = bytes / 4;

  for (size_t i = 0; i < length16; i++) {
    dataLeft[i] = (data16Bit[i * 2] / (float)MAX_INT16);  // Normalize left
    dataRight[i] =
        (data16Bit[i * 2 + 1] / (float)MAX_INT16);  // Normalize right
  }
  float* sampleData[] = {&dataLeft[0], &dataRight[0]};
  streamInfo->data = sampleData;

  if (activePipeline) {
    streamInfo = activePipeline->process(std::move(streamInfo));
  }

  if (this->underflowEffect != nullptr &&
      this->underflowEffect->duration >= samplesLeftInBuffer + length16) {
    this->underflowEffect->apply(dataLeft.data(), length16, samplesLeftInBuffer);

    if (streamInfo->numChannels > 1) {
      this->underflowEffect->apply(dataRight.data(), length16, samplesLeftInBuffer);
    }

  }
  for (size_t i = 0; i < length16; i++) {
    if (dataLeft[i] > 1.0f) {
      dataLeft[i] = 1.0f;
    }

    // Data has been downmixed to mono
    if (streamInfo->numChannels == 1) {

      data16Bit[i] = dataLeft[i] * MAX_INT16;  // Denormalize left
    } else {
      data16Bit[i * 2] = dataLeft[i] * MAX_INT16;       // Denormalize left
      data16Bit[i * 2 + 1] = dataRight[i] * MAX_INT16;  // Denormalize right
    }
  }

  if (streamInfo->numChannels == 1) {
    return bytes / 2;
  }

  return bytes;
}

std::shared_ptr<AudioPipeline> BellDSP::getActivePipeline() {
  return activePipeline;
}
