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
    float sampleFrequency = 44100.0;

public:
    BiquadFilter(){};

    BiquadFilter(int sample_frequence)
    {
      sampleFrequency = sample_frequence;
    }

    // Generates coefficients for a high pass biquad filter
    void generateHighPassCoEffs(float f, float q){
        q = safeMin(q);

    //    EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: f: %.2f, q: %.2f", f, q);
//        float Fs = 1;

        float w0 = calcW0(f);
  //      EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: w0,: %.2f", w0);
        float c = cosf(w0);
  //      EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: c,: %.2f", c);
        float alpha = calcAlpha(w0,q);
  //      EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew:alpha,: %.2f", alpha);

        float b0 = (1 + c) / 2;
        float b1 = -(1 + c);
        float b2 = b0;
        float a0 = 1 + alpha;
        float a1 = -2 * c;
        float a2 = 1 - alpha;

/*
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: b0,: %.2f", b0);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: b1,: %.2f", b1);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: b2,: %.2f", b2);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: a0,: %.2f", a0);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: a1,: %.2f", a1);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffsNew: a2,: %.2f", a2);
        */

        setCoefss(a0, a1, a2, b0, b1, b2);
    }

    // Generates coefficients for a high pass biquad filter
    void generateHighPassCoEffsOld(float f, float q){
        if (q <= 0.0001) {
            q = 0.0001;
        }
        float Fs = 1;

        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: f: %.2f, q: %.2f", f, q);

        float w0 = 2 * M_PI * (f/sampleFrequency) / Fs;
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: w0,: %.2f", w0);
        float c = cosf(w0);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: c,: %.2f", c);
        float s = sinf(w0);
        float alpha = s / (2 * q);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: alpha,: %.2f", alpha);

        float b0 = (1 + c) / 2;
        float b1 = -(1 + c);
        float b2 = b0;
        float a0 = 1 + alpha;
        float a1 = -2 * c;
        float a2 = 1 - alpha;

        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: b0,: %.2f", b0);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: b1,: %.2f", b1);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: b2,: %.2f", b2);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: a0,: %.2f", a0);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: a1,: %.2f", a1);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighPassCoEffs: a2,: %.2f", a2);

        coeffs[0] = b0 / a0;
        coeffs[1] = b1 / a0;
        coeffs[2] = b2 / a0;
        coeffs[3] = a1 / a0;
        coeffs[4] = a2 / a0;
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

        setCoefss(a0, a1, a2, b0, b1, b2);
    }

    // Generates coefficients for a high shelf biquad filter
    void generateHighShelfCoEffsOld(float f, float gain, float q)
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

    void generateHighShelfCoEffs(float f, float gain, float q)
    {
        // Formula from here: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: f: %.2f, gain: %.2f, q: %.6f", f, gain, q);
        q = safeMin(q);
//        float Fs = 1;

        float A = calcA(gain);
        float w0 = calcW0(f);
        float c = cosf(w0);
        float alpha = calcAlpha(w0,q);

        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: f: %.2f, q: %.6f", f, q);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: w0,: %.2f", w0);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: c,: %.2f", c);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: alpha,: %.2f", alpha);

        float b0 = A * ((A + 1) + ((A - 1) * c) + (2 * sqrtf(A) * alpha));
        float b1 = -2 * A * ((A - 1) + ((A + 1) * c));
        float b2 = A * ((A + 1) + ((A - 1) * c) - (2 * sqrtf(A) * alpha));
        float a0 = (A + 1) - ((A - 1) * c) + (2 * sqrtf(A) * alpha);
        float a1 = 2 * ((A - 1) - ((A + 1) * c));
        float a2 = (A + 1) - ((A - 1) * c) - (2 * sqrtf(A) * alpha);

        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: b0,: %.6f", b0);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: b1,: %.6f", b1);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: b2,: %.6f", b2);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: a0,: %.6f", a0);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: a1,: %.6f", a1);
        EUPH_LOG(debug, "eq", "BiquadFilters; generateHighShelfCoEffs: a2,: %.6f", a2);

        setCoefss(a0, a1, a2, b0, b1, b2);
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

    float safeMin(float q)
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
      return 2 * M_PI * (f / sampleFrequency);
    }

    float calcA(float gain)
    {
      return sqrtf(pow(10, (double)gain / 20.0));
    }


    // Is this a copy or a reference?
    void setCoefss(float a0, float a1, float a2, float b0, float b1, float b2)
    {
      std::scoped_lock lock(processMutex);

      float coeffs_b0 = b0 / a0;
      float coeffs_b1 = b1 / a0;
      float coeffs_b2 = b2 / a0;
      float coeffs_a1 = a1 / a0;
      float coeffs_a2 = a2 / a0;

      EUPH_LOG(debug, "eq", "BiquadFilters; setCoefss: b0,: %.6f", coeffs_b0);
      EUPH_LOG(debug, "eq", "BiquadFilters; setCoefss: b1,: %.6f", coeffs_b1);
      EUPH_LOG(debug, "eq", "BiquadFilters; setCoefss: b2,: %.6f", coeffs_b2);
      EUPH_LOG(debug, "eq", "BiquadFilters; setCoefss: a0,: %.6f", coeffs_a1);
      EUPH_LOG(debug, "eq", "BiquadFilters; setCoefss: a1,: %.6f", coeffs_a2);

      coeffs[0] = coeffs_b0;
      coeffs[1] = coeffs_b1;
      coeffs[2] = coeffs_b2;
      coeffs[3] = coeffs_a1;
      coeffs[4] = coeffs_a2;
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
