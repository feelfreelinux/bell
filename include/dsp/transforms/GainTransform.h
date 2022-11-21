#pragma once

#include <cmath>
#include <mutex>

#include "AudioTransform.h"

namespace bell
{
    class GainTransform : public bell::AudioTransform
    {
    private:
    float gainFactor = 1.0f;

    bell::Channels channel;

    public:
        GainTransform(Channels channels);
        ~GainTransform() {};
        
        float gainDb = 0.0;
        
        void configure(float gainDB);

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;

        float calculateHeadroom() override { return gainDb; };
    };
}