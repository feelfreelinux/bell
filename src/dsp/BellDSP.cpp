#include "BellDSP.h"

using namespace bell::dsp;

BellDSP::BellDSP(){

};

void BellDSP::applyPipeline(std::shared_ptr<AudioPipeline> pipeline)
{
    std::scoped_lock lock(accessMutex);
    activePipeline = pipeline;
}

void BellDSP::process(uint8_t *data, size_t bytes, int channels, SampleRate sampleRate, BitWidth bitWidth)
{
    // Create a StreamInfo object to pass to the pipeline
    auto streamInfo = std::make_unique<StreamInfo>();
    streamInfo->numChannels = channels;
    streamInfo->sampleRate = sampleRate;
    streamInfo->bitwidth = bitWidth;

    std::scoped_lock lock(accessMutex);
    if (activePipeline)
    {
        int16_t *data16Bit = (int16_t *)data;

        int length16 = bytes / 4;

        for (size_t i = 0; i < length16; i++)
        {
            dataLeft[i] = data16Bit[i * 2] / (float)MAX_INT16; // Normalize left
            dataRight[i] =
                data16Bit[i * 2 + 1] / (float)MAX_INT16; // Normalize right
        }
        float* sampleData[] = {&dataLeft[0], &dataRight[0]};
        streamInfo->data = sampleData;

        activePipeline->process(std::move(streamInfo));
    }
}