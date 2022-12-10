#include "AudioPipeline.h"
#include <iostream>

using namespace bell;

AudioPipeline::AudioPipeline() {
    // this->headroomGainTransform = std::make_shared<Gain>(Channels::LEFT_RIGHT);
    // this->transforms.push_back(this->headroomGainTransform);
};

void AudioPipeline::addTransform(std::shared_ptr<AudioTransform> transform) {
    transforms.push_back(transform);
    recalculateHeadroom();
}

void AudioPipeline::recalculateHeadroom() {
    float headroom = 0.0f;

    // Find largest headroom required by any transform down the chain, and apply it
    for (auto transform : transforms) {
        if (headroom < transform->calculateHeadroom()) {
            headroom = transform->calculateHeadroom();
        }
    }

    // headroomGainTransform->configure(-headroom);
}

std::unique_ptr<StreamInfo> AudioPipeline::process(std::unique_ptr<StreamInfo> data) {
    for (auto &transform : transforms) {
        data = transform->process(std::move(data));
    }

    return data;
}