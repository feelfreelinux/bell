#pragma once

#include <cmath>
#include <mutex>

#include "AudioTransform.h"
extern "C" int dsps_biquad_f32_ae32(const float* input, float* output, int len, float* coef, float* w);

namespace bell
{
    class BiquadTransform : public bell::AudioTransform
    {
    public:
        enum class Type
        {
            HighPass,
            LowPass,
            LowShelf,
            HighShelf,
            Notch,
            Peak,
            PeakEQ,
            Pass180,
            Pass360,
        };

        BiquadTransform(BiquadTransform::Type type, Channels channels);

        float freq, q, gain;
        BiquadTransform::Type type;

        void configure(float frequency, float q, float gain);
        void configureWithSampleRate(float frequency, float q, float gain);

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;
        void sampleRateChanged(SampleRate sampleRate) override;

        float calculateHeadroom() override { return this->gain; };

    private:
        float coeffs[5];
        float w[2] = {0, 0};

        int sampleRate = 44100;
        bool dynamicSampleRate = false;

        bell::Channels channel;

        void generateHighPassCoEffs(float f, float q);
        void generateLowPassCoEffs(float f, float q);

        void generateHighShelfCoEffs(float f, float gain, float q);
        void generateLowShelfCoEffs(float f, float gain, float q);

        void generateNotchCoEffs(float f, float gain, float q);

        void generatePeakCoEffs(float f, float gain, float q);
        void generatePeakingEqCoEffs(float f, float gain, float q);
        
        void generateAllPass180CoEffs(float f, float q);
        void generateAllPass360CoEffs(float f, float q);
    };

}