#pragma once

#include <vector>
#include <memory>
#include <cmath>
#include <mutex>
#include <map>

#include "Biquad.h"
#include "AudioTransform.h"

namespace bell
{
    class BiquadCombo : public bell::AudioTransform
    {
    private:
        std::vector<std::unique_ptr<bell::Biquad>> biquads;

        // Calculates Q values for Nth order Butterworth / Linkwitz-Riley filters
        std::vector<float> calculateBWQ(int order);
        std::vector<float> calculateLRQ(int order);

    public:
        BiquadCombo();
        ~BiquadCombo(){};
        int channel;

        enum class FilterType
        {
            Highpass,
            Lowpass
        };

        void linkwitzRiley(float freq, int order, FilterType type);
        void butterworth(float freq, int order, FilterType type);

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;
        void sampleRateChanged(uint32_t sampleRate) override;

        void reconfigure() override
        {
            this->channel = config->getChannels()[0];
            float freq = config->getFloat("freq");
            int order = config->getInt("order");

            auto type = config->getString("type");
            if (type == "lr_lowpass")
            {
                this->linkwitzRiley(freq, order, FilterType::Lowpass);
            }
            else if (type == "lr_highpass")
            {
                this->linkwitzRiley(freq, order, FilterType::Highpass);
            }
            else if (type == "bw_highpass")
            {
                this->butterworth(freq, order, FilterType::Highpass);
            }
            else if (type == "bw_lowpass")
            {
                this->butterworth(freq, order, FilterType::Highpass);
            }
            else
            {
                throw std::invalid_argument("Invalid combo filter type");
            }
        }
    };
};