#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include "StreamInfo.h"

namespace bell::dsp
{
    class AudioTransform
    {
    protected:
        std::mutex accessMutex;
    public:
        virtual std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) = 0;
        virtual void sampleRateChanged(SampleRate sampleRate);
        virtual int calculateHeadroom() = 0;
        virtual ~AudioTransform() = default;
    };
};