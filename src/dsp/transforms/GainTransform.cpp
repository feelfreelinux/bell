#include "GainTransform.h"
#include <iostream>

using namespace bell;

GainTransform::GainTransform(Channels channels) : AudioTransform() {
    this->channel = channels;
    this->gainFactor = 1.0f;
}

void GainTransform::configure(float gainDB) {
    std::scoped_lock lock(this->accessMutex);
    this->gainDb = gainDB;
    this->gainFactor = std::pow(10.0f, gainDB / 20.0f);
}

std::unique_ptr<StreamInfo>
GainTransform::process(std::unique_ptr<StreamInfo> data) {
    std::scoped_lock lock(this->accessMutex);
    if (this->channel == bell::Channels::LEFT) {
        for (int i = 0; i < data->numSamples; i++) {
            data->data[0][i] *= this->gainFactor;
        }
    } else if (this->channel == bell::Channels::RIGHT) {
        for (int i = 0; i < data->numSamples; i++) {
            data->data[1][i] *= this->gainFactor;
        }

    } else if (this->channel == bell::Channels::LEFT_RIGHT) {
        for (int i = 0; i < data->numSamples; i++) {
            data->data[0][i] *= this->gainFactor;
            data->data[1][i] *= this->gainFactor;
        }
    }

    return data;
}