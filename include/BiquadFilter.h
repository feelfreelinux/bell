#ifndef BELL_BIQUADFILTER_H
#define BELL_BIQUADFILTER_H

#include <mutex>
#include <cmath>
#include "esp_platform.h"

extern "C" int dsps_biquad_f32_ae32(const float* input, float* output, int len, float* coef, float* w);

class BiquadFilter
{
private:
    std::mutex processMutex;
    float coeffs[5];
    float w[2] = {0, 0};
    float Fs = 1;

public:
    BiquadFilter(){};

    // Generates coefficients for a high pass biquad filter
    void generateHighPassCoEffs(float f, float q){
        q = safeMin(q);
//        float Fs = 1;

        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        float b0 = (1 + c) / 2;
        float b1 = -(1 + c);
        float b2 = b0;
        float a0 = 1 + alpha;
        float a1 = -2 * c;
        float a2 = 1 - alpha;

        setCoefss(a0,a1, a2, b0, b1, b2);
    }

    // Generates coefficients for a low pass biquad filter
    void generateLowPassCoEffs(float f, float q){
        q = safeMin(q);

//        float Fs = 1;

        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        float b0 = (1 - c) / 2;
        float b1 = 1 - c;
        float b2 = b0;
        float a0 = 1 + alpha;
        float a1 = -2 * c;
        float a2 = 1 - alpha;

        setCoefss(a0,a1, a2, b0, b1, b2);
    }

    // Generates coefficients for a high shelf biquad filter
    void generateHighShelfCoEffs(float f, float gain, float q)
    {
        q = safeMin(q);
//        float Fs = 1;

        float A = sqrtf(pow(10, (double)gain / 20.0));
        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        float b0 = A * ((A + 1) + (A - 1) * c + 2 * sqrtf(A) * alpha);
        float b1 = -2 * A * ((A - 1) + (A + 1) * c);
        float b2 = A * ((A + 1) + (A - 1) * c - 2 * sqrtf(A) * alpha);
        float a0 = (A + 1) - (A - 1) * c + 2 * sqrtf(A) * alpha;
        float a1 = 2 * ((A - 1) - (A + 1) * c);
        float a2 = (A + 1) - (A - 1) * c - 2 * sqrtf(A) * alpha;

        setCoefss(a0,a1, a2, b0, b1, b2);
    }

    // Generates coefficients for a low shelf biquad filter
    void generateLowShelfCoEffs(float f, float gain, float q)
    {
        q = safeMin(q);
//        float Fs = 1;

        float A = sqrtf(pow(10, (double)gain / 20.0));
        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        float b0 = A * ((A + 1) - (A - 1) * c + 2 * sqrtf(A) * alpha);
        float b1 = 2 * A * ((A - 1) - (A + 1) * c);
        float b2 = A * ((A + 1) - (A - 1) * c - 2 * sqrtf(A) * alpha);
        float a0 = (A + 1) + (A - 1) * c + 2 * sqrtf(A) * alpha;
        float a1 = -2 * ((A - 1) + (A + 1) * c);
        float a2 = (A + 1) + (A - 1) * c - 2 * sqrtf(A) * alpha;

        setCoefss(a0,a1, a2, b0, b1, b2);
    }

    // Generates coefficients for a notch biquad filter
    void generateNotchCoEffs(float f, float gain, float q)
    {
        q = safeMin(q);
//        float Fs = 1;

        float A = sqrtf(pow(10, (double)gain / 20.0));
        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        float b0 = 1 + alpha * A;
        float b1 = -2 * c;
        float b2 = 1 - alpha * A;
        float a0 = 1 + alpha;
        float a1 = -2 * c;
        float a2 = 1 - alpha;

        setCoefss(a0,a1, a2, b0, b1, b2);

    }

    void generateLowPassCoeffs(float f, float gain, float q)
    {
      // Formula from here: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html

      q = safeMin(q);
//      float Fs = 1;

      float w0 = calcW0(f);
      float c = cosf(w0);
      float alpha = calcAlpha(w0,q);

      // coefficients
      float b0 = (1 - c) / 2;
      float b1 = 1 - c;
      float b2 = b0;
      float a0 = 1 + alpha;
      float a1 = -2 * c;
      float a2 = a0;

      setCoefss(a0,a1, a2, b0, b1, b2);
    }



    // Generates coefficients for a peaking biquad filter
    void generatePeakCoEffs(float f, float gain, float q)
    {
        q = safeMin(q);
//        float Fs = 1;

        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        float b0 = alpha;
        float b1 = 0;
        float b2 = -alpha;
        float a0 = 1 + alpha;
        float a1 = -2 * c;
        float a2 = 1 - alpha;

        setCoefss(a0,a1, a2, b0, b1, b2);
    }

    // Generates coefficients for an all pass 180° biquad filter
    void generateAllPass180CoEffs(float f,  float q)
    {
        q = safeMin(q);
//        float Fs = 1;

        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        float b0 = 1 - alpha;
        float b1 = -2 * c;
        float b2 = 1 + alpha;
        float a0 = 1 + alpha;
        float a1 = -2 * c;
        float a2 = 1 - alpha;

        setCoefss(a0,a1, a2, b0, b1, b2);
    }

    // Generates coefficients for an all pass 360° biquad filter
    void generateAllPass360CoEffs(float f,  float q)
    {
        q = safeMin(q);
//        float Fs = 1;

        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        float b0 = 1 - alpha;
        float b1 = -2 * c;
        float b2 = 1 + alpha;
        float a0 = 1 + alpha;
        float a1 = -2 * c;
        float a2 = 1 - alpha;

        setCoefss(a0,a1, a2, b0, b1, b2);
    }

    float safeMin(int q)
    {
      if (q <= 0.0001)
      {
          return 0.0001;
      }
      return q;
    }

    float calcAlpha(float w0, float q)
    {
      return sinf(w0) / (2 * q);
    }

    float calcW0(float f)
    {
      return 2 * M_PI * f / Fs;
    }


    // Is this a copy or a reference?
    void setCoefss(float a0, float a1, float a2, float b0, float b1, float b2)
    {
      std::scoped_lock lock(processMutex);
      coeffs[0] = b0 / a0;
      coeffs[1] = b1 / a0;
      coeffs[2] = b2 / a0;
      coeffs[3] = a1 / a0;
      coeffs[4] = a2 / a0;
    }

    void processSamples(float *input, int numSamples)
    {
        std::scoped_lock lock(processMutex);

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
    }
};

#endif
