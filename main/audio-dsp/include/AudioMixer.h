#pragma once

#include <cJSON.h>    // for cJSON_GetObjectItem, cJSON, cJSON_IsArray
#include <stddef.h>   // for NULL
#include <algorithm>  // for find
#include <cstdint>    // for uint8_t
#include <memory>     // for unique_ptr
#include <stdexcept>  // for invalid_argument
#include <vector>     // for vector

#include "AudioTransform.h"  // for AudioTransform
#include "StreamInfo.h"      // for StreamInfo
#include "TransformConfig.h"

namespace bell {
class AudioMixer : public bell::AudioTransform {
 public:
  enum DownmixMode { DEFAULT };

  AudioMixer();
  ~AudioMixer(){};
  // Amount of channels in the input
  int from;

  // Amount of channels in the output
  int to;

  // Configuration of each channels in the mixer
  std::vector<TransformConfig::MixerConfig> mixerConfig;

  std::unique_ptr<StreamInfo> process(
      std::unique_ptr<StreamInfo> data) override;

  void reconfigure() override {
    this->mixerConfig = config->rawGetMixerConfig("mapped_channels");

    std::vector<uint8_t> sources(0);
    for (auto& config : mixerConfig) {

      for (auto& source : config.source) {
        if (std::find(sources.begin(), sources.end(), source) ==
            sources.end()) {
          sources.push_back(source);
        }
      }
    }

    this->from = 2;
    this->to = 2;  // TODO: set it to actual number fo destinations
  }

  void sampleRateChanged(uint32_t sampleRate) override {}
};
}  // namespace bell