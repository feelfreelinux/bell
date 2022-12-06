#include "Biquad.h"

using namespace bell;

Biquad::Biquad()
{
    this->filterType = "biquad";
}

void Biquad::sampleRateChanged(uint32_t sampleRate)
{
    this->sampleRate = sampleRate;
    this->configure(this->type, this->currentConfig);
}

void Biquad::configure(Type type, std::map<std::string, float> &config)
{
    this->type = type;
    this->currentConfig = config;

    switch (type)
    {
    case Type::Free:
        coeffs[0] = config["a1"];
        coeffs[1] = config["a2"];
        coeffs[2] = config["b0"];
        coeffs[3] = config["b1"];
        coeffs[4] = config["b2"];
        break;
    case Type::Highpass:
        highPassCoEffs(config["freq"], config["q"]);
        break;
    case Type::HighpassFO:
        highPassFOCoEffs(config["freq"]);
        break;
    case Type::Lowpass:
        lowPassCoEffs(config["freq"], config["q"]);
        break;
    case Type::LowpassFO:
        lowPassFOCoEffs(config["freq"]);
        break;
    case Type::Highshelf:
        // check if config has slope key
        if (config.find("slope") != config.end())
        {
            highShelfCoEffsSlope(config["freq"], config["gain"], config["slope"]);
        }
        else
        {
            highShelfCoEffs(config["freq"], config["gain"], config["q"]);
        }
        break;
    case Type::HighshelfFO:
        highShelfFOCoEffs(config["freq"], config["gain"]);
        break;
    case Type::Lowshelf:
        // check if config has slope key
        if (config.find("slope") != config.end())
        {
            lowShelfCoEffsSlope(config["freq"], config["gain"], config["slope"]);
        }
        else
        {
            lowShelfCoEffs(config["freq"], config["gain"], config["q"]);
        }
        break;
    case Type::LowshelfFO:
        lowShelfFOCoEffs(config["freq"], config["gain"]);
        break;
    case Type::Peaking:
        // check if config has bandwidth key
        if (config.find("bandwidth") != config.end())
        {
            peakCoEffsBandwidth(config["freq"], config["gain"], config["bandwidth"]);
        }
        else
        {
            peakCoEffs(config["freq"], config["gain"], config["q"]);
        }
        break;
    case Type::Notch:
        if (config.find("bandwidth") != config.end())
        {
            notchCoEffsBandwidth(config["freq"], config["gain"], config["bandwidth"]);
        }
        else
        {
            notchCoEffs(config["freq"], config["gain"], config["q"]);
        }
        break;
    case Type::Bandpass:
        if (config.find("bandwidth") != config.end())
        {
            bandPassCoEffsBandwidth(config["freq"], config["bandwidth"]);
        }
        else
        {
            bandPassCoEffs(config["freq"], config["q"]);
        }
        break;
    case Type::Allpass:
        if (config.find("bandwidth") != config.end())
        {
            allPassCoEffsBandwidth(config["freq"], config["bandwidth"]);
        }
        else
        {
            allPassCoEffs(config["freq"], config["q"]);
        }
        break;
    case Type::AllpassFO:
        allPassFOCoEffs(config["freq"]);
        break;
    }
}

// coefficients for a high pass biquad filter
void Biquad::highPassCoEffs(float f, float q)
{
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = (1 + c) / 2;
    float b1 = -(1 + c);
    float b2 = b0;
    float a0 = 1 + alpha;
    float a1 = -2 * c;
    float a2 = 1 - alpha;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

// coefficients for a high pass first order biquad filter
void Biquad::highPassFOCoEffs(float f)
{
    float w0 = 2 * M_PI * f / this->sampleRate;
    float k = tanf(w0 / 2.0);

    float alpha = 1.0 + k;

    float b0 = 1.0 / alpha;
    float b1 = -1.0 / alpha;
    float b2 = 0.0;
    float a0 = 1.0;
    float a1 = -(1.0 - k) / alpha;
    float a2 = 0.0;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

// coefficients for a low pass biquad filter
void Biquad::lowPassCoEffs(float f, float q)
{
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float b0 = (1 - c) / 2;
    float b1 = 1 - c;
    float b2 = b0;
    float a0 = 1 + alpha;
    float a1 = -2 * c;
    float a2 = 1 - alpha;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

// coefficients for a low pass first order biquad filter
void Biquad::lowPassFOCoEffs(float f)
{
    float w0 = 2 * M_PI * f / this->sampleRate;
    float k = tanf(w0 / 2.0);

    float alpha = 1.0 + k;

    float b0 = k / alpha;
    float b1 = k / alpha;
    float b2 = 0.0;
    float a0 = 1.0;
    float a1 = -(1.0 - k) / alpha;
    float a2 = 0.0;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

// coefficients for a peak biquad filter
void Biquad::peakCoEffs(float f, float gain, float q)
{
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);

    float ampl = std::pow(10.0f, gain / 40.0f);
    float b0 = 1.0 + (alpha * ampl);
    float b1 = -2.0 * c;
    float b2 = 1.0 - (alpha * ampl);
    float a0 = 1 + (alpha / ampl);
    float a1 = -2 * c;
    float a2 = 1 - (alpha / ampl);

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void Biquad::peakCoEffsBandwidth(float f, float gain, float bandwidth)
{
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s * sinh(logf(2.0) / 2.0 * bandwidth * w0 / s);

    float ampl = std::pow(10.0f, gain / 40.0f);
    float b0 = 1.0 + (alpha * ampl);
    float b1 = -2.0 * c;
    float b2 = 1.0 - (alpha * ampl);
    float a0 = 1 + (alpha / ampl);
    float a1 = -2 * c;
    float a2 = 1 - (alpha / ampl);

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void Biquad::highShelfCoEffs(float f, float gain, float q)
{
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2 * q);
    float beta = s * sqrtf(A) / q;
    float b0 = A * ((A + 1.0) + (A - 1.0) * c + beta);
    float b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * c);
    float b2 = A * ((A + 1.0) + (A - 1.0) * c - beta);
    float a0 = (A + 1.0) - (A - 1.0) * c + beta;
    float a1 = 2.0 * ((A - 1.0) - (A + 1.0) * c);
    float a2 = (A + 1.0) - (A - 1.0) * c - beta;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void Biquad::highShelfCoEffsSlope(float f, float gain, float slope)
{
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha =
        s / 2.0 * sqrtf((A + 1.0 / A) * (1.0 / (slope / 12.0) - 1.0) + 2.0);
    float beta = 2.0 * sqrtf(A) * alpha;
    float b0 = A * ((A + 1.0) + (A - 1.0) * c + beta);
    float b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * c);
    float b2 = A * ((A + 1.0) + (A - 1.0) * c - beta);
    float a0 = (A + 1.0) - (A - 1.0) * c + beta;
    float a1 = 2.0 * ((A - 1.0) - (A + 1.0) * c);
    float a2 = (A + 1.0) - (A - 1.0) * c - beta;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void Biquad::highShelfFOCoEffs(float f, float gain)
{
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2 * M_PI * f / this->sampleRate;
    float tn = tanf(w0 / 2.0);

    float b0 = A * tn + std::pow(A, 2);
    float b1 = A * tn - std::pow(A, 2);
    float b2 = 0.0;
    float a0 = A * tn + 1.0;
    float a1 = A * tn - 1.0;
    float a2 = 0.0;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void Biquad::lowShelfCoEffs(float f, float gain, float q) {
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float beta = s * sqrtf(A) / q;

    float b0 = A * ((A + 1.0) - (A - 1.0) * c + beta);
    float b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * c);
    float b2 = A * ((A + 1.0) - (A - 1.0) * c - beta);
    float a0 = (A + 1.0) + (A - 1.0) * c + beta;
    float a1 = -2.0 * ((A - 1.0) + (A + 1.0) * c);
    float a2 = (A + 1.0) + (A - 1.0) * c - beta;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void Biquad::lowShelfCoEffsSlope(float f, float gain, float slope) {
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha =
        s / 2.0 * sqrtf((A + 1.0 / A) * (1.0 / (slope / 12.0) - 1.0) + 2.0);
    float beta = 2.0 * sqrtf(A) * alpha;

    float b0 = A * ((A + 1.0) - (A - 1.0) * c + beta);
    float b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * c);
    float b2 = A * ((A + 1.0) - (A - 1.0) * c - beta);
    float a0 = (A + 1.0) + (A - 1.0) * c + beta;
    float a1 = -2.0 * ((A - 1.0) + (A + 1.0) * c);
    float a2 = (A + 1.0) + (A - 1.0) * c - beta;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void Biquad::lowShelfFOCoEffs(float f, float gain) {
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2 * M_PI * f / this->sampleRate;
    float tn = tanf(w0 / 2.0);

    float b0 = std::pow(A, 2) * tn + A;
    float b1 = std::pow(A, 2) * tn - A;
    float b2 = 0.0;
    float a0 = tn + A;
    float a1 = tn - A;
    float a2 = 0.0;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void Biquad::notchCoEffs(float f, float gain, float q) {
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2.0 * q);

    float b0 = 1.0;
    float b1 = -2.0 * c;
    float b2 = 1.0;
    float a0 = 1.0 + alpha;
    float a1 = -2.0 * c;
    float a2 = 1.0 - alpha;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void Biquad::notchCoEffsBandwidth(float f, float gain, float bandwidth) {
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s * sinh(logf(2.0) / 2.0 * bandwidth * w0 / s);

    float b0 = 1.0;
    float b1 = -2.0 * c;
    float b2 = 1.0;
    float a0 = 1.0 + alpha;
    float a1 = -2.0 * c;
    float a2 = 1.0 - alpha;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void Biquad::bandPassCoEffs(float f, float q) {
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2.0 * q);

    float b0 = alpha;
    float b1 = 0.0;
    float b2 = -alpha;
    float a0 = 1.0 + alpha;
    float a1 = -2.0 * c;
    float a2 = 1.0 - alpha;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void Biquad::bandPassCoEffsBandwidth(float f, float bandwidth) {
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s * sinh(logf(2.0) / 2.0 * bandwidth * w0 / s);

    float b0 = alpha;
    float b1 = 0.0;
    float b2 = -alpha;
    float a0 = 1.0 + alpha;
    float a1 = -2.0 * c;
    float a2 = 1.0 - alpha;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void Biquad::allPassCoEffs(float f, float q) {
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2.0 * q);

    float b0 = 1.0 - alpha;
    float b1 = -2.0 * c;
    float b2 = 1.0 + alpha;
    float a0 = 1.0 + alpha;
    float a1 = -2.0 * c;
    float a2 = 1.0 - alpha;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void Biquad::allPassCoEffsBandwidth(float f, float bandwidth) {
    float w0 = 2 * M_PI * f / this->sampleRate;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s * sinh(logf(2.0) / 2.0 * bandwidth * w0 / s);

    float b0 = 1.0 - alpha;
    float b1 = -2.0 * c;
    float b2 = 1.0 + alpha;
    float a0 = 1.0 + alpha;
    float a1 = -2.0 * c;
    float a2 = 1.0 - alpha;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void Biquad::allPassFOCoEffs(float f) {
    float w0 = 2 * M_PI * f / this->sampleRate;
    float tn = tanf(w0 / 2.0);

    float alpha = (tn + 1.0) / (tn - 1.0);

    float b0 = 1.0;
    float b1 = alpha; 
    float b2 = 0.0;
    float a0 = alpha;
    float a1 = 1.0;
    float a2 = 0.0;

    this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void Biquad::normalizeCoEffs(float a0, float a1, float a2, float b0, float b1, float b2)
{
    std::scoped_lock lock(accessMutex);
    coeffs[0] = b0 / a0;
    coeffs[1] = b1 / a0;
    coeffs[2] = b2 / a0;
    coeffs[3] = a1 / a0;
    coeffs[4] = a2 / a0;
}

std::unique_ptr<StreamInfo> Biquad::process(std::unique_ptr<StreamInfo> stream)
{
    std::scoped_lock lock(accessMutex);

    auto input = stream->data[this->channel];
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
