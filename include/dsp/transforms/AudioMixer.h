#pragma once

#include <vector>
#include <algorithm>
#include <cJSON.h>

#include "AudioTransform.h"

namespace bell
{
    class AudioMixer : public bell::AudioTransform
    {
    public:
        enum DownmixMode
        {
            DEFAULT
        };

        struct MixerConfig
        {
            std::vector<int> source;
            int destination;
        };

        AudioMixer();
        ~AudioMixer(){};
        // Amount of channels in the input
        int from;

        // Amount of channels in the output
        int to;

        // Configuration of each channels in the mixer
        std::vector<MixerConfig> config;

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;

        float calculateHeadroom() override { return 0; };

        void fromJSON(cJSON *json) override
        {
            cJSON *mappedChannels = cJSON_GetObjectItem(json, "mapped_channels");

            if (mappedChannels == NULL || !cJSON_IsArray(mappedChannels))
            {
                throw std::invalid_argument("Mixer configuration invalid");
            }

            this->config = std::vector<MixerConfig>();

            cJSON *iterator = NULL;
            cJSON_ArrayForEach(iterator, mappedChannels)
            {
                std::vector<int> sources(0);
                cJSON *iteratorNested = NULL;
                cJSON_ArrayForEach(iteratorNested, cJSON_GetObjectItem(iterator, "source"))
                {
                    sources.push_back(iteratorNested->valueint);
                }

                this->config.push_back(MixerConfig{
                    .source = sources,
                    .destination = jsonGetNumber<int>(iterator, "destination", true),
                });
            }

            std::vector<uint8_t> sources(0);

            for (auto &config : config)
            {

                for (auto &source : config.source)
                {
                    if (std::find(sources.begin(), sources.end(), source) == sources.end())
                    {
                        sources.push_back(source);
                    }
                }
            }

            this->from = sources.size();
            this->to = config.size();
        }
    };
}