#pragma once

#include <cmath>
#include <mutex>

#include "AudioTransform.h"

namespace bell
{
    class MonoDownmixTransform : public bell::AudioTransform
    {
    private:


    public:
        MonoDownmixTransform();
        ~MonoDownmixTransform() {};

        std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) override;

        float calculateHeadroom() override { return 0; };
    };
}