#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include "StreamInfo.h"

namespace bell
{
    class AudioTransform
    {
    protected:
        std::mutex accessMutex;
    public:
        virtual std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) = 0;
        virtual void sampleRateChanged(SampleRate sampleRate) {};
        virtual float calculateHeadroom() { return 0; };
        
        AudioTransform() = default;
        virtual ~AudioTransform() = default;
    };
};