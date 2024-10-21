#include "BellDSP.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <type_traits>  // for remove_extent_t
#include <utility>      // for move

#include "AudioPipeline.h"       // for CentralAudioBuffer
#include "CentralAudioBuffer.h"  // for CentralAudioBuffer
#include "StreamInfo.h"

using namespace bell;

BellDSP::FadeEffect::FadeEffect(size_t duration, bool isFadeIn,
                                std::function<void()> onFinish) {
  this->duration = duration;
  this->onFinish = onFinish;
  this->isFadeIn = isFadeIn;
}

BellDSP::~BellDSP() {}

void BellDSP::FadeEffect::apply(float* audioData, size_t samples,
                                size_t relativePosition) {
  float effect = (this->duration - relativePosition) / (float)this->duration;

  if (isFadeIn) {
    effect = relativePosition / (float)this->duration;
  }

  for (int x = 0; x <= samples; x++) {
    audioData[x] *= effect;
  }

  if (relativePosition + samples > this->duration && onFinish != nullptr) {
    onFinish();
  }
}

BellDSP::BellDSP() {
  dataSlots = std::make_shared<DSPDataSlots>();

  // insert channel 0 and 1
  dataSlots->insert({0, std::array<float, 2048>()});
  dataSlots->insert({1, std::array<float, 2048>()});
};

void BellDSP::applyPipeline(std::shared_ptr<AudioPipeline> pipeline) {
  std::scoped_lock lock(accessMutex);
  activePipeline = pipeline;
}

void BellDSP::queryInstantEffect(std::unique_ptr<AudioEffect> instantEffect) {
  this->instantEffect = std::move(instantEffect);
  samplesSinceInstantQueued = 0;
}

std::unique_ptr<StreamInfo> BellDSP::process(
    const uint8_t* inputData, size_t inputDataLen, uint8_t* outputBuffer,
    size_t outputBufferLen, int channels, SampleRate sampleRate,
    BitWidth bitWidth) {
  // Create a StreamInfo object to pass to the pipeline
  auto streamInfo = std::make_unique<StreamInfo>();
  streamInfo->numChannels = channels;
  streamInfo->sampleRate = static_cast<bell::SampleRate>(sampleRate);
  streamInfo->bitwidth = bitWidth;
  streamInfo->numSamples =
      inputDataLen / channels / (static_cast<int>(bitWidth) / 8);
  assert(streamInfo->numSamples <= (dspMaxFrames / 2));
  assert(streamInfo->numChannels <= dspMaxChannels);

  std::scoped_lock lock(accessMutex);

  auto data16 = reinterpret_cast<const int16_t*>(inputData);
  auto data32 = reinterpret_cast<const int32_t*>(inputData);

  // Normalize data into floats, for processing
  for (size_t frameIdx = 0; frameIdx < streamInfo->numSamples; frameIdx++) {
    for (auto chan = 0; chan < streamInfo->numChannels; chan++) {
      switch (bitWidth) {
        case BitWidth::BW_16: {
          dataSlots->at(chan)[frameIdx] =
              data16[(frameIdx * streamInfo->numChannels) + chan] /
              (float)std::numeric_limits<int16_t>::max();
          break;
        }
        case BitWidth::BW_32: {
          dataSlots->at(chan)[frameIdx] =
              data32[(frameIdx * streamInfo->numChannels) + chan] /
              (float)std::numeric_limits<int32_t>::max();
          break;
        }
        default:
          break;
      }
    }
  }

  streamInfo->data = dataSlots;

  if (activePipeline) {
    // Run frames through the provided pipeline
    streamInfo = activePipeline->process(std::move(streamInfo));
  }

  // Instant effect is acive, apply
  if (this->instantEffect != nullptr) {
    for (int chan = 0; chan < streamInfo->numChannels; chan++) {
      this->instantEffect->apply(dataSlots->at(chan).data(),
                                 streamInfo->numSamples,
                                 samplesSinceInstantQueued);
    }

    samplesSinceInstantQueued += streamInfo->numSamples;

    if (this->instantEffect->duration <= samplesSinceInstantQueued) {
      this->instantEffect = nullptr;
    }
  }

  auto outputData16 = reinterpret_cast<int16_t*>(outputBuffer);
  auto outputData32 = reinterpret_cast<int32_t*>(outputBuffer);

  // Denormalize frames back into PCM data
  for (size_t frameIdx = 0; frameIdx < streamInfo->numSamples; frameIdx++) {
    for (auto chan = 0; chan < streamInfo->numChannels; chan++) {
      // Clip data
      if (dataSlots->at(chan)[frameIdx] > 1.0f) {
        dataSlots->at(chan)[frameIdx] = 1.0f;
      }

      switch (bitWidth) {
        case BitWidth::BW_16:
          outputData16[(frameIdx * streamInfo->numChannels) + chan] =
              dataSlots->at(chan)[frameIdx] *
              (float)std::numeric_limits<int16_t>::max();
          break;
        case BitWidth::BW_32:
          outputData32[(frameIdx * streamInfo->numChannels) + chan] =
              dataSlots->at(chan)[frameIdx] *
              (float)std::numeric_limits<int32_t>::max();
          break;
        default:
          break;
      }
    }
  }

  return streamInfo;
}

std::shared_ptr<AudioPipeline> BellDSP::getActivePipeline() {
  return activePipeline;
}
