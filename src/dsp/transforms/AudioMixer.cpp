#include "AudioMixer.h"

using namespace bell;

AudioMixer::AudioMixer() {
}

std::unique_ptr<StreamInfo> AudioMixer::process(std::unique_ptr<StreamInfo> info) {
    if (info->numChannels != from) {
        throw std::runtime_error("AudioMixer: Input channel count does not match configuration");
    }
    info->numChannels = to;

    for (auto &config : config) {
        if (config.source.size() == 1) {
            if (config.source[0] == config.destination) {
                continue;
            }
            // Copy channel
            for (int i = 0; i < info->numSamples; i++) {
                info->data[config.destination][i] = info->data[config.source[0]][i];
            }
        } else {
            // Mix channels
            float sample = 0.0f;
            for (int i = 0; i < info->numSamples; i++) {
                sample = 0.0;
                for (auto &source : config.source) {
                    sample += info->data[source][i];
                }
                
                info->data[config.destination][i] = sample / (float) config.source.size();
            }
        }
    }

    return info;
}