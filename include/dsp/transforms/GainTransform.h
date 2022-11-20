#pragma once

#include <cmath>
#include <mutex>

#include "AudioTransform.h"

namespace bell
{
    class GainTransform : public bell::AudioTransform
    {
    private:
    float gainFactor;
    float gainDb;

    bell::Channels channel;

    public:
        GainTransform(Channels channels);
        ~GainTransform() {};

        void configure(float gainDB);

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;

        float calculateHeadroom() override { return gainDb; };
    };
}