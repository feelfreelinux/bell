#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <mutex>
#include <map>

#include "Biquad.h"
#include "AudioTransform.h"

namespace bell
{
    class Compressor : public bell::AudioTransform
    {
    private:
        std::vector<int> channels;
        std::vector<float> tmp;

        std::map<std::string, float> paramCache;

        float attack;
        float release;
        float threshold;
        float factor;
        float clipLimit;
        float makeupGain;

        float lastLoudness = -100.0f;

        float sampleRate = 44100;

    public:
        Compressor();
        ~Compressor(){};

        void configure(std::vector<int> channels, float attack, float release, float threshold, float factor, float makeupGain);

        void sumChannels(std::unique_ptr<StreamInfo> &data);
        void calLoudness();
        void calGain();

        void applyGain(std::unique_ptr<StreamInfo> &data);

        void reconfigure() override
        {
            std::scoped_lock lock(this->accessMutex);
            auto newChannels = config->getChannels();
            
            float newAttack = config->getFloat("attack");
            float newRelease = config->getFloat("release");
            float newThreshold = config->getFloat("threshold");
            float newFactor = config->getFloat("factor");
            float newMakeupGain = config->getFloat("makeup_gain");

            if (paramCache["attack"] == newAttack &&
                paramCache["release"] == newRelease &&
                paramCache["threshold"] == newThreshold &&
                paramCache["factor"] == newFactor &&
                paramCache["makeup_gain"] == newMakeupGain)
            {
                return;
            }
            else
            {

                paramCache["attack"] = newAttack;
                paramCache["release"] = newRelease;
                paramCache["threshold"] = newThreshold;
                paramCache["factor"] = newFactor;
                paramCache["makeup_gain"] = newMakeupGain;
            }

            this->configure(newChannels, newAttack, newRelease, newThreshold, newFactor, newMakeupGain);
        }

        // void fromJSON(cJSON* json) override {
        //     // get field channels
        //     channels = jsonGetChannels(json);
        //     float attack = jsonGetNumber<float>(json, "attack", false, 0);
        //     float release = jsonGetNumber<float>(json, "release", false, 0);
        //     float factor = jsonGetNumber<float>(json, "factor", false, 4);
        //     float makeupGain = jsonGetNumber<float>(json, "makeup_gain", false, 0);
        //     float threshold = jsonGetNumber<float>(json, "threshold", false, 0);

        //     this->configure(attack, release, clipLimit, threshold, factor, makeupGain);
        // }

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;
        void sampleRateChanged(uint32_t sampleRate) override { this->sampleRate = sampleRate; };
    };
};