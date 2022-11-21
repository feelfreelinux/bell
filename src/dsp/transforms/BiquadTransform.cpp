#include "BiquadTransform.h"

using namespace bell;

BiquadTransform::BiquadTransform(BiquadTransform::Type type, Channels channels)
{
    this->type = type;
    this->channel = channels;
    this->filterType = "biquad";
}

void BiquadTransform::sampleRateChanged(SampleRate sampleRate)
{
    this->sampleRate = sampleRate == SampleRate::SR_44100 ? 44100 : 48000;

    if (this->dynamicSampleRate) {
        this->configureWithSampleRate(this->freq, this->q, this->gain);
    }
}

void BiquadTransform::configureWithSampleRate(float frequency, float q, float gain) {
    this->configure(frequency / (float) this->sampleRate, q, gain);
    this->freq = frequency;
    this->dynamicSampleRate = true;
}

void BiquadTransform::configure(float frequency, float q, float gain)
{
    this->freq = frequency;
    this->q = q;
    this->gain = gain;
    this->dynamicSampleRate = false;

    switch (type)
    {
    case Type::HighPass:
        generateHighPassCoEffs(frequency, q);
        break;
    case Type::LowPass:
        generateLowPassCoEffs(frequency, q);
        break;
    case Type::HighShelf:
        generateHighShelfCoEffs(frequency, gain, q);
        break;
    case Type::LowShelf:
        generateLowShelfCoEffs(frequency, gain, q);
        break;
    case Type::Notch:
        generateNotchCoEffs(frequency, gain, q);
        break;
    case Type::Peak:
        generatePeakCoEffs(frequency, gain, q);
        break;
    case Type::PeakEQ:
        generatePeakingEqCoEffs(frequency, gain, q);
        break;
    case Type::Pass180:
        generateAllPass180CoEffs(frequency, q);
        break;
    case Type::Pass360:
        generateAllPass360CoEffs(frequency, q);
        break;
    }
}

// Generates coefficients for a high pass biquad filter
void BiquadTransform::generateHighPassCoEffs(float f, float q)
{
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = (1 + c) / 2;
    float b1 = -(1 + c);
    float b2 = b0;
    float a0 = 1 + alpha;
    float a1 = -2 * c;
    float a2 = 1 - alpha;

    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

// Generates coefficients for a low pass biquad filter
void BiquadTransform::generateLowPassCoEffs(float f, float q)
{
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = (1 - c) / 2;
    float b1 = 1 - c;
    float b2 = b0;
    float a0 = 1 + alpha;
    float a1 = -2 * c;
    float a2 = 1 - alpha;

    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

// Generates coefficients for a high shelf biquad filter
void BiquadTransform::generateHighShelfCoEffs(float f, float gain, float q)
{
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float A = sqrtf(pow(10, (double)gain / 20.0));
    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = A * ((A + 1) + (A - 1) * c + 2 * sqrtf(A) * alpha);
    float b1 = -2 * A * ((A - 1) + (A + 1) * c);
    float b2 = A * ((A + 1) + (A - 1) * c - 2 * sqrtf(A) * alpha);
    float a0 = (A + 1) - (A - 1) * c + 2 * sqrtf(A) * alpha;
    float a1 = 2 * ((A - 1) - (A + 1) * c);
    float a2 = (A + 1) - (A - 1) * c - 2 * sqrtf(A) * alpha;

    std::lock_guard lock(accessMutex);
    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

// Generates coefficients for a low shelf biquad filter
void BiquadTransform::generateLowShelfCoEffs(float f, float gain, float q)
{
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float A = sqrtf(pow(10, (double)gain / 20.0));
    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = A * ((A + 1) - (A - 1) * c + 2 * sqrtf(A) * alpha);
    float b1 = 2 * A * ((A - 1) - (A + 1) * c);
    float b2 = A * ((A + 1) - (A - 1) * c - 2 * sqrtf(A) * alpha);
    float a0 = (A + 1) + (A - 1) * c + 2 * sqrtf(A) * alpha;
    float a1 = -2 * ((A - 1) + (A + 1) * c);
    float a2 = (A + 1) + (A - 1) * c - 2 * sqrtf(A) * alpha;

    std::lock_guard lock(accessMutex);
    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

// Generates coefficients for a notch biquad filter
void BiquadTransform::generateNotchCoEffs(float f, float gain, float q)
{
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float A = sqrtf(pow(10, (double)gain / 20.0));
    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = 1 + alpha * A;
    float b1 = -2 * c;
    float b2 = 1 - alpha * A;
    float a0 = 1 + alpha;
    float a1 = -2 * c;
    float a2 = 1 - alpha;

    std::scoped_lock lock(accessMutex);
    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

// Generates coefficients for a peaking biquad filter
void BiquadTransform::generatePeakCoEffs(float f, float gain, float q)
{
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = alpha;
    float b1 = 0;
    float b2 = -alpha;
    float a0 = 1 + alpha;
    float a1 = -2 * c;
    float a2 = 1 - alpha;

    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

// Generates coefficients for a peaking EQ biquad filter
void BiquadTransform::generatePeakingEqCoEffs(float f, float gain, float q)
{
    // formular taken from: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);
    float A = sqrtf(pow(10, (double)gain / 20.0));

    float b0 = 1 + (alpha * A);
    float b1 = (-2 * c);
    float b2 = 1 - (alpha * A);
    float a0 = 1 + (alpha / A);
    float a1 = b1;
    float a2 = 1 - (alpha / A);

    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

// Generates coefficients for an all pass 180° biquad filter
void BiquadTransform::generateAllPass180CoEffs(float f, float q)
{
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = 1 - alpha;
    float b1 = -2 * c;
    float b2 = 1 + alpha;
    float a0 = 1 + alpha;
    float a1 = -2 * c;
    float a2 = 1 - alpha;

    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

// Generates coefficients for an all pass 360° biquad filter
void BiquadTransform::generateAllPass360CoEffs(float f, float q)
{
    if (q <= 0.0001)
    {
        q = 0.0001;
    }
    float Fs = 1;

    float w0 = 2 * M_PI * f / Fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = 1 - alpha;
    float b1 = -2 * c;
    float b2 = 1 + alpha;
    float a0 = 1 + alpha;
    float a1 = -2 * c;
    float a2 = 1 - alpha;

    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

std::unique_ptr<StreamInfo> BiquadTransform::process(std::unique_ptr<StreamInfo> stream)
{
    std::scoped_lock lock(accessMutex);
    int chanIndex = channel == Channels::RIGHT ? 1 : 0;

    auto input = stream->data[chanIndex];
    auto numSamples = stream->numSamples;

#ifdef ESP_PLATFORM
    dsps_biquad_f32_ae32(input, input, numSamples, coeffs, w);
#else
    // Apply the set coefficients
    for (int i = 0; i < numSamples; i++)
    {
        float d0 = input[i] - coeffs[3] * w[0] - coeffs[4] * w[1];
        input[i] = coeffs[0] * d0 + coeffs[1] * w[0] + coeffs[2] * w[1];
        w[1] = w[0];
        w[0] = d0;
    }
#endif
    
    return stream;
};
