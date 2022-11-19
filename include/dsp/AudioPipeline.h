#pragma once

#include "AudioTransform.h"
#include "StreamInfo.h"
#include <memory>

namespace bell::dsp {
class AudioPipeline {
  private:
    std::vector<std::unique_ptr<AudioTransform>> transforms;

  public:
    AudioPipeline();
    ~AudioPipeline(){};
    void addTransform(std::unique_ptr<AudioTransform> transform);
    std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data);
};
}; // namespace bell::dsp