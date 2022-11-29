#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
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

        void configure(float attack, float release, float clipLimit, float threshold, float factor, float makeupGain);

        void sumChannels(std::unique_ptr<StreamInfo> &data);
        void calLoudness();
        void calGain();

        void applyGain(std::unique_ptr<StreamInfo> &data);
        void applyLimiter(std::unique_ptr<StreamInfo> &data);

        void fromJSON(cJSON* json) override {
            // get field channels
            channels = jsonGetChannels(json);
            float attack = jsonGetNumber<float>(json, "attack", false, 0);
            float release = jsonGetNumber<float>(json, "release", false, 0);
            float clipLimit = jsonGetNumber<float>(json, "clip_limit", false, -4);
            float factor = jsonGetNumber<float>(json, "factor", false, 4);
            float makeupGain = jsonGetNumber<float>(json, "makeupGain", false, 0);

            this->configure(attack, release, clipLimit, threshold, factor, makeupGain);
        }

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;
        void sampleRateChanged(uint32_t sampleRate) override { this->sampleRate = sampleRate; };
        float calculateHeadroom() override { return 0.0f; };
    };
};