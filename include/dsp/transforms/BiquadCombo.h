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

        std::map<std::string, float> paramCache = {
            {"order", 0.0f},
            {"frequency", 0.0f}
        };

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
            std::scoped_lock lock(this->accessMutex);

            float freq = config->getFloat("frequency");
            int order = config->getInt("order");

            if (paramCache["frequency"] == freq && paramCache["order"] == order)
            {
                return;
            } else {
                paramCache["frequency"] = freq;
                paramCache["order"] = order;
            }

            this->channel = config->getChannels()[0];
            this->biquads = std::vector<std::unique_ptr<bell::Biquad>>();
            auto type = config->getString("combo_type");
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