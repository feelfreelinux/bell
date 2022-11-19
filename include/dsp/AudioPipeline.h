#pragma once

#include <memory>
#include "StreamInfo.h"
#include "AudioTransform.h"

namespace bell::dsp
{
    class AudioPipeline
    {
    private:
        std::vector<std::unique_ptr<AudioTransform>> transforms;
    public:
        void addTransform(std::unique_ptr<AudioTransform> transform);
        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data);
    };
};