#include "AudioMixer.h"

#include <mutex>  // for scoped_lock

using namespace bell;

AudioMixer::AudioMixer() {}

std::unique_ptr<StreamInfo> AudioMixer::process(
    std::unique_ptr<StreamInfo> info) {
  std::scoped_lock lock(this->accessMutex);

  if (info->numChannels != from) {
    throw std::runtime_error(
        "AudioMixer: Input channel count does not match configuration");
  }
  info->numChannels = to;

  for (auto& singleConf : mixerConfig) {
    if (singleConf.source.size() == 1) {
      if (singleConf.source[0] == singleConf.destination) {
        continue;
      }
      // Copy channel
      for (int i = 0; i < info->numSamples; i++) {
        info->data->at(singleConf.destination)[i] =
            info->data->at(singleConf.source[0])[i];
      }
    } else {
      // Mix channels
      for (int i = 0; i < info->numSamples; i++) {
        float sample = 0.0f;
        for (auto& source : singleConf.source) {
          sample += info->data->at(source)[i];
        }

        info->data->at(singleConf.destination)[i] =
            sample / (float)singleConf.source.size();
      }
    }
  }

  return info;
}