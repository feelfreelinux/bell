#include "AudioPipeline.h"

using namespace bell::dsp;

AudioPipeline::AudioPipeline() {};

void AudioPipeline::addTransform(std::unique_ptr<AudioTransform> transform) {
    transforms.push_back(std::move(transform));
}

std::unique_ptr<StreamInfo> AudioPipeline::process(std::unique_ptr<StreamInfo> data) {
    for (auto &transform : transforms) {
        data = transform->process(std::move(data));
    }

    return data;
}