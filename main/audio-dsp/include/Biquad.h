#pragma once

#include <cmath>
#include <mutex>
#include <map>
#include <unordered_map>

#include "AudioTransform.h"
extern "C" int dsps_biquad_f32_ae32(const float *input, float *output, int len, float *coef, float *w);

namespace bell
{
    class Biquad : public bell::AudioTransform
    {
    public:
        Biquad();
        ~Biquad(){};

        enum class Type
        {
            Free,
            Highpass,
            Lowpass,
            HighpassFO,
            LowpassFO,

            Peaking,
            Highshelf,
            HighshelfFO,
            Lowshelf,
            LowshelfFO,
            Notch,
            Bandpass,
            Allpass,
            AllpassFO
        };

        std::map<std::string, float> config;

        std::unordered_map<std::string, Type> const strMapType = {
            {"free", Type::Free},
            {"highpass", Type::Highpass},
            {"lowpass", Type::Lowpass},
            {"highpass_fo", Type::HighpassFO},
            {"lowpass_fo", Type::LowpassFO},
            {"peaking", Type::Peaking},
            {"highshelf", Type::Highshelf},
            {"highshelf_fo", Type::HighpassFO},
            {"lowshelf", Type::Lowshelf},
            {"lowshelf_fo", Type::LowpassFO},
            {"notch", Type::Notch},
            {"bandpass", Type::Bandpass},
            {"allpass", Type::Allpass},
            {"allpass_fo", Type::AllpassFO},
        };

        float freq, q, gain;
        int channel;
        Biquad::Type type;

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;

        void configure(Type type, std::map<std::string, float> &config);
        
        void sampleRateChanged(uint32_t sampleRate) override;

        float calculateHeadroom() override { return this->gain; };

        void fromJSON(cJSON *json) override
        {
            // this->channel = jsonGetChannels(json)[0];
            // auto type = jsonGetString(json, "biquad_type");
            // std::map<std::string, float> biquadConfig;

            // float invalid = -0x7C;
            // float bandwidth = jsonGetNumber<float>(json, "bandwidth", false, invalid);
            // float slope = jsonGetNumber<float>(json, "slope", false, invalid);
            // float gain = jsonGetNumber<float>(json, "gain", false, invalid);
            // float frequency = jsonGetNumber<float>(json, "frequency", false, invalid);
            // float q = jsonGetNumber<float>(json, "q", false, invalid);

            // if (bandwidth != invalid)
            //     biquadConfig["bandwidth"] = bandwidth;
            // if (slope != invalid)
            //     biquadConfig["slope"] = slope;
            // if (gain != invalid)
            //     biquadConfig["gain"] = gain;
            // if (frequency != invalid)
            //     biquadConfig["freq"] = frequency;
            // if (q != invalid)
            //     biquadConfig["q"] = q;

            // if (type == "free")
            // {
            //     biquadConfig["a1"] = jsonGetNumber<float>(json, "a1", false);
            //     biquadConfig["a2"] = jsonGetNumber<float>(json, "a2", false);
            //     biquadConfig["b0"] = jsonGetNumber<float>(json, "b0", false);
            //     biquadConfig["b1"] = jsonGetNumber<float>(json, "b1", false);
            //     biquadConfig["b2"] = jsonGetNumber<float>(json, "b2", false);
            // }

            // auto typeElement = strMapType.find(type);
            // if (typeElement != strMapType.end())
            // {
            //     this->configure(typeElement->second, biquadConfig);
            // }
            // else
            // {
            //     throw std::invalid_argument("No biquad of type " + type);
            // }
        }

    private:
        float coeffs[5];
        float w[2] = {1.0, 1.0};

        float sampleRate = 44100;

        // Generator methods for different filter types
        void highPassCoEffs(float f, float q);
        void highPassFOCoEffs(float f);
        void lowPassCoEffs(float f, float q);
        void lowPassFOCoEffs(float f);

        void peakCoEffs(float f, float gain, float q);
        void peakCoEffsBandwidth(float f, float gain, float bandwidth);

        void highShelfCoEffs(float f, float gain, float q);
        void highShelfCoEffsSlope(float f, float gain, float slope);
        void highShelfFOCoEffs(float f, float gain);

        void lowShelfCoEffs(float f, float gain, float q);
        void lowShelfCoEffsSlope(float f, float gain, float slope);
        void lowShelfFOCoEffs(float f, float gain);

        void notchCoEffs(float f, float gain, float q);
        void notchCoEffsBandwidth(float f, float gain, float bandwidth);

        void bandPassCoEffs(float f, float q);
        void bandPassCoEffsBandwidth(float f, float bandwidth);

        void allPassCoEffs(float f, float q);
        void allPassCoEffsBandwidth(float f, float bandwidth);
        void allPassFOCoEffs(float f);

        void normalizeCoEffs(float a0, float a1, float a2, float b0, float b1, float b2);
    };

}