#pragma once

#include "AudioTransform.h"
#include "StreamInfo.h"
#include <memory>
#include "GainTransform.h"

namespace bell
{
  class AudioPipeline
  {
  private:
    std::shared_ptr<GainTransform> headroomGainTransform;

  public:
    AudioPipeline();
    ~AudioPipeline(){};

    std::vector<std::shared_ptr<AudioTransform>> transforms;

    void recalculateHeadroom();
    void addTransform(std::shared_ptr<AudioTransform> transform);
    std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data);
  };
}; // namespace bell