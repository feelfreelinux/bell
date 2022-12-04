#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include "StreamInfo.h"
#include "cJSON.h"

namespace bell
{
    class AudioTransform
    {
    protected:
        std::mutex accessMutex;

        std::string jsonGetString(cJSON* body, std::string field) {
            // cJSON *value = cJSON_GetObjectItem(body, field.c_str());

            // if (value != NULL && cJSON_IsString(value)) {
            //     return std::string(value->valuestring);
            // }

            return "";
        }

        template <class T>
        T jsonGetNumber(cJSON *body, std::string fieldName, bool throws = false, T defaultValue = 0)
        {
            // cJSON *value = cJSON_GetObjectItem(body, fieldName.c_str());

            // if (value == NULL || !cJSON_IsNumber(value))
            // {
            //     if (throws)
            //     {
            //         throw std::invalid_argument("Field " + fieldName + " missing or not number");
            //     }
            //     else
            //     {
            //         return defaultValue;
            //     }
            // }

            return (T)0;
        }

        /**
         * @brief Parses 'channels' or 'channel' field
         *
         * @param body json body of the config
         * @return std::vector<int> target audio channels
         */
        std::vector<int> jsonGetChannels(cJSON *body)
        {
            std::vector<int> res;
            // int chan = jsonGetNumber<int>(body, "channel", false, -1);

            // if (chan > -1) {
            //     res.push_back(chan);
            // }
            
            // cJSON *channels = cJSON_GetObjectItem(body, "channels");

            // if (cJSON_IsArray(channels))
            // {
            //     cJSON *iterator = NULL;
            //     cJSON_ArrayForEach(iterator, channels)
            //     {
            //         if (cJSON_IsNumber(iterator))
            //         {
            //             res.push_back(iterator->valueint);
            //         }
            //     }
            //}
            return res;
        }

    public:
        virtual std::unique_ptr<StreamInfo> process(std::unique_ptr<StreamInfo> data) = 0;
        virtual void sampleRateChanged(uint32_t sampleRate){};
        virtual float calculateHeadroom() { return 0; };

        virtual void fromJSON(cJSON *) {};

        std::string filterType;

        AudioTransform() = default;
        virtual ~AudioTransform() = default;
    };
};