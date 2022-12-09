#pragma once

#include <cmath>
#include <mutex>
#include <iostream>

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

        void reconfigure() override {
            std::scoped_lock lock(this->accessMutex);
            float gain = config->getFloat("gain");
            this->channels = config->getChannels();

            if (gainDb == gain) {
                return;
            }

            this->configure(channels, gain);
        }
    };
}