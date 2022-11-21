#include "MonoDownmixTransform.h"
#include <iostream>

using namespace bell;

MonoDownmixTransform::MonoDownmixTransform() : AudioTransform()
{
    this->filterType = "monodownmix";
}

std::unique_ptr<StreamInfo> MonoDownmixTransform::process(std::unique_ptr<StreamInfo> data)
{
    std::scoped_lock lock(this->accessMutex);
    if (data->numChannels == 1)
    {
        return data;
    }
    else
    {
        // Perform downmix to mono
        for (int i = 0; i < data->numSamples; i++)
        {
            data->data[0][i] = (data->data[0][i] + data->data[1][i]) / 2;
        }
        data->numChannels = 1;
        return data;
    }

    return data;
}