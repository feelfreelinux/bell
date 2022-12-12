#include "BellDSP.h"
#include <iostream>
#include "CentralAudioBuffer.h"

using namespace bell;

BellDSP::BellDSP(std::shared_ptr<CentralAudioBuffer> buffer)
{
    this->buffer = buffer;
};

void BellDSP::applyPipeline(std::shared_ptr<AudioPipeline> pipeline)
{
    std::scoped_lock lock(accessMutex);
    activePipeline = pipeline;
}

size_t BellDSP::process(uint8_t *data, size_t bytes, int channels,
                        SampleRate sampleRate, BitWidth bitWidth)
{
    if (bytes > 1024 * 2 * channels)
    {
        throw std::runtime_error("Too many bytes");
    }
    
    auto bytesLeftInBuffer = buffer->audioBuffer->size();
    size_t fadeoutBytes = 44100;

    // Create a StreamInfo object to pass to the pipeline
    auto streamInfo = std::make_unique<StreamInfo>();
    streamInfo->numChannels = channels;
    streamInfo->sampleRate = sampleRate;
    streamInfo->bitwidth = bitWidth;
    streamInfo->numSamples = bytes / channels / 2;

    std::scoped_lock lock(accessMutex);
    if (activePipeline)
    {
        int16_t *data16Bit = (int16_t *)data;

        int length16 = bytes / 4;

        for (size_t i = 0; i < length16; i++)
        {
            dataLeft[i] = (data16Bit[i * 2] / (float)MAX_INT16); // Normalize left
            dataRight[i] =
                (data16Bit[i * 2 + 1] / (float)MAX_INT16); // Normalize right
        }
        float *sampleData[] = {&dataLeft[0], &dataRight[0]};
        streamInfo->data = sampleData;

        auto resultInfo = activePipeline->process(std::move(streamInfo));

        for (size_t i = 0; i < length16; i++)
        {
            if (dataLeft[i] > 1.0f)
            {
                dataLeft[i] = 1.0f;
            }

            // Data has been downmixed to mono
            if (resultInfo->numChannels == 1)
            {
                size_t actualBytesInBuffer = (bytesLeftInBuffer + bytes);
                actualBytesInBuffer -= (length16 * 4) - (i * 4);

                data16Bit[i] = dataLeft[i] * MAX_INT16; // Denormalize left
                if (actualBytesInBuffer < fadeoutBytes)
                {
                    int steps = (actualBytesInBuffer * 33) / fadeoutBytes;
                    data16Bit[i] *= (float)steps / 33;
                }
            }
            else
            {
                data16Bit[i * 2] = dataLeft[i] * MAX_INT16;      // Denormalize left
                data16Bit[i * 2 + 1] = dataRight[i] * MAX_INT16; // Denormalize right
            }
        }

        if (resultInfo->numChannels == 1)
        {
            return bytes / 2;
        }
    }

    return bytes;
}

std::shared_ptr<AudioPipeline> BellDSP::getActivePipeline()
{
    return activePipeline;
}