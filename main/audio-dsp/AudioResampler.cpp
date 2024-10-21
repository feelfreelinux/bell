#include "AudioResampler.h"
#include <cassert>
#include "StreamInfo.h"
#include "speex_resampler.h"

using namespace bell;

AudioResampler::AudioResampler() {}

AudioResampler::~AudioResampler() {
  if (resampler != nullptr) {
    speex_resampler_destroy(resampler);
  }
}

void AudioResampler::configure(int channels, SampleRate from, SampleRate to) {
  this->from = from;
  this->to = to;

  if (resampler != nullptr) {
    speex_resampler_destroy(resampler);
  }

  int err = 0;
  resampler = speex_resampler_init(channels, static_cast<int>(from),
                                   static_cast<int>(to),
                                   SPEEX_RESAMPLER_QUALITY_DEFAULT, &err);
  if (resampler == NULL) {
    throw std::runtime_error("Could not initialize speex resampler");
  }
}

std::unique_ptr<StreamInfo> AudioResampler::process(
    std::unique_ptr<StreamInfo> info) {
  std::scoped_lock lock(this->accessMutex);
  assert(resampler != nullptr);

  // Assert that bandwidth matches config
  assert(from == info->sampleRate);

  float resampledOutput[info->numSamples * 2];
  spx_uint32_t outputSize = info->numSamples * 2;

  for (int chanIdx = 0; chanIdx < info->numChannels; chanIdx++) {
    spx_uint32_t inputSize = info->numSamples;

    speex_resampler_process_float(resampler, chanIdx,
                                  info->data->at(chanIdx).data(), &inputSize,
                                  resampledOutput, &outputSize);

    std::copy(&resampledOutput[0], &resampledOutput[outputSize],
              &info->data->at(chanIdx)[0]);
  }
  info->numSamples = outputSize;

  return info;
}