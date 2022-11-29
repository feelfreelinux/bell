#include <iostream>
#include <cmath>
#include <vector>
#include <memory.h>
#include <map>
#include <fstream>

#include <BellLogger.h>
#include <BellDSP.h>
#include <AudioPipeline.h>
#include <Biquad.h>
#include <Gain.h>
#include <Compressor.h>
#include <StreamInfo.h>
#include <AudioMixer.h>
#include <cJSON.h>
#include <BiquadCombo.h>
#include <FileStream.h>

std::vector<uint8_t> generate_sine_wave(int sample_rate, int channels, int seconds, int bits_per_sample)
{
    std::vector<uint8_t> data;
    data.reserve(sample_rate * channels * seconds * bits_per_sample / 8);
    for (int i = 0; i < sample_rate * seconds; i++)
    {
        double t = (double)i / (double)sample_rate;
        double v = sin(t * 2.0 * M_PI * 1000.0);
        int16_t sample = (int16_t)(v * 32767.0);
        data.push_back(sample & 0xff);
        data.push_back((sample >> 8) & 0xff);

        data.push_back(sample & 0xff);
        data.push_back((sample >> 8) & 0xff);
    }
    return data;
}

int main()
{
    bell::setDefaultLogger();
    std::ifstream file2("radiohead.pcm", std::ios::binary | std::ios::ate);
    std::streamsize size2 = file2.tellg();
    file2.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer2(size2);
    if (!file2.read((char*) buffer2.data(), size2))
    {
        throw std::invalid_argument("pipeline file not found");
    }

    std::ifstream file("pipeline.json", std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size))
    {
        throw std::invalid_argument("pipeline file not found");
    }

    // cJSON parse buffer
    cJSON *root = cJSON_Parse(buffer.data());
    cJSON *array = cJSON_GetObjectItem(root, "transforms");
    auto pipeline = std::make_shared<bell::AudioPipeline>();

    cJSON *iterator = NULL;
    cJSON_ArrayForEach(iterator, array)
    {
        cJSON *type = cJSON_GetObjectItem(iterator, "type");
        if (type == NULL || !cJSON_IsString(type))
            continue;

        auto typeStr = std::string(type->valuestring);

        if (typeStr == "gain")
        {
            auto filter = std::make_shared<bell::Gain>();
            filter->fromJSON(iterator);
            pipeline->addTransform(filter);
        }
        else if (typeStr == "compressor")
        {
            auto filter = std::make_shared<bell::Compressor>();
            filter->fromJSON(iterator);
            pipeline->addTransform(filter);
        }
        else if (typeStr == "mixer")
        {
            auto filter = std::make_shared<bell::AudioMixer>();
            filter->fromJSON(iterator);
            pipeline->addTransform(filter);
        }
        else if (typeStr == "biquad")
        {
            auto filter = std::make_shared<bell::Biquad>();
            filter->fromJSON(iterator);
            pipeline->addTransform(filter);
        }
        else if (typeStr == "biquad_combo")
        {
            auto filter = std::make_shared<bell::BiquadCombo>();
            filter->fromJSON(iterator);
            pipeline->addTransform(filter);
        }
        else
        {
            throw std::invalid_argument("No filter type found for configuration field " + typeStr);
        }
    }

    auto dsp = std::make_shared<bell::BellDSP>();

    dsp->applyPipeline(pipeline);

    size_t total = 0;
    size_t totalOut = 0;
    // read sinewave in 1024 byte chunks
    FILE *f = fopen("out.pcm", "wb");
    for (int i = 0; i < buffer2.size(); i += 1024)
    {
        int bytes = buffer2.size() - i;
        if (bytes > 1024)
        {
            bytes = 1024;
        }
        int res = dsp->process(&buffer2[i], bytes, 2, bell::SampleRate::SR_44100, bell::BitWidth::BW_16);
        fwrite(&buffer2[i], 1, res, f);
    }

    // write sinewave to out.pcm
    fclose(f);

    return 0;
}
