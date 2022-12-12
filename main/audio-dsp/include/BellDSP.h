#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include "AudioPipeline.h"
#include "CentralAudioBuffer.h"

namespace bell
{
    #define MAX_INT16 32767
    
    class BellDSP
    {

    private:
        std::shared_ptr<AudioPipeline> activePipeline;
        std::shared_ptr<CentralAudioBuffer> buffer;
        std::mutex accessMutex;
        std::vector<float> dataLeft = std::vector<float>(1024);
        std::vector<float> dataRight = std::vector<float>(1024);

    public:
        BellDSP(std::shared_ptr<CentralAudioBuffer> centralAudioBuffer);
        ~BellDSP(){};

        void applyPipeline(std::shared_ptr<AudioPipeline> pipeline);

        std::shared_ptr<AudioPipeline> getActivePipeline();

        size_t process(uint8_t *data, size_t bytes, int channels, SampleRate sampleRate, BitWidth bitWidth);
    };
};