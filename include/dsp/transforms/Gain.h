#pragma once

#include <cmath>
#include <mutex>

#include "AudioTransform.h"

namespace bell
{
    class Gain : public bell::AudioTransform
    {
    private:
    float gainFactor = 1.0f;

    std::vector<int> channels;

    public:
        Gain();
        ~Gain() {};
        
        float gainDb = 0.0;
        
        void configure(std::vector<int> channels, float gainDB);

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;

        float calculateHeadroom() override { return gainDb; };

        void fromJSON(cJSON* json) override {
            // get field channels
            auto channels = jsonGetChannels(json);
            float gain = jsonGetNumber<float>(json, "gain", true);
            this->configure(channels, gain);
        }
    };
}