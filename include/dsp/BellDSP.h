#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include "AudioPipeline.h"

namespace bell
{
    #define MAX_INT16 32767
    
    class BellDSP
    {

    private:
        std::shared_ptr<AudioPipeline> activePipeline;
        std::mutex accessMutex;
        std::vector<float> dataLeft = std::vector<float>(1024);
        std::vector<float> dataRight = std::vector<float>(1024);

    public:
        BellDSP();
        ~BellDSP(){};

        void applyPipeline(std::shared_ptr<AudioPipeline> pipeline);

        void process(uint8_t *data, size_t bytes, int channels, SampleRate sampleRate, BitWidth bitWidth);
    };
};