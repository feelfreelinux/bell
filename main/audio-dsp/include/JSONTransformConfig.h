#pragma once

#include "TransformConfig.h"
#include "cJSON.h"

namespace bell {
class JSONTransformConfig : public bell::TransformConfig {
 private:
  cJSON* json;

 public:
  JSONTransformConfig(cJSON* body) { this->json = body; };
  ~JSONTransformConfig(){};

  std::string rawGetString(const std::string& field) override {
    cJSON* value = cJSON_GetObjectItem(json, field.c_str());

    if (value != NULL && cJSON_IsString(value)) {
      return std::string(value->valuestring);
    }

    return "invalid";
  }

  std::vector<MixerConfig> rawGetMixerConfig(
      const std::string& field) override {
    cJSON* mappedChannels = cJSON_GetObjectItem(json, field.c_str());

    if (mappedChannels == NULL || !cJSON_IsArray(mappedChannels)) {
      throw std::invalid_argument("Mixer configuration invalid");
    }

    std::vector<MixerConfig> result;

    cJSON* iterator = NULL;
    cJSON_ArrayForEach(iterator, mappedChannels) {
      std::vector<int> sources(0);
      cJSON* iteratorNested = NULL;
      cJSON_ArrayForEach(iteratorNested,
                         cJSON_GetObjectItem(iterator, "source")) {
        sources.push_back(iteratorNested->valueint);
      }

      int destination = cJSON_GetObjectItem(iterator, "destination")->valueint;

      result.push_back(
          MixerConfig{.source = sources, .destination = destination});
    }

    return result;
  }

  std::vector<int> rawGetIntArray(const std::string& field) override {
    std::vector<int> result;

    cJSON* value = cJSON_GetObjectItem(json, field.c_str());

    if (value != NULL && cJSON_IsArray(value)) {
      for (int i = 0; i < cJSON_GetArraySize(value); i++) {
        cJSON* item = cJSON_GetArrayItem(value, i);
        if (item != NULL && cJSON_IsNumber(item)) {
          result.push_back(item->valueint);
        }
      }
    }

    return result;
  }

  std::vector<float> rawGetFloatArray(const std::string& field) override {
    std::vector<float> result;

    cJSON* value = cJSON_GetObjectItem(json, field.c_str());

    if (value != NULL && cJSON_IsArray(value)) {
      for (int i = 0; i < cJSON_GetArraySize(value); i++) {
        cJSON* item = cJSON_GetArrayItem(value, i);
        if (item != NULL && cJSON_IsNumber(item)) {
          result.push_back(item->valuedouble);
        }
      }
    }

    return result;
  }

  int rawGetInt(const std::string& field) override {
    cJSON* value = cJSON_GetObjectItem(json, field.c_str());

    if (value != NULL && cJSON_IsNumber(value)) {
      return (int)value->valueint;
    }

    return invalidInt;
  }

  bool isArray(const std::string& field) override {
    cJSON* value = cJSON_GetObjectItem(json, field.c_str());

    if (value != NULL && cJSON_IsArray(value)) {
      return true;
    }

    return false;
  }

  float rawGetFloat(const std::string& field) override {
    cJSON* value = cJSON_GetObjectItem(json, field.c_str());

    if (value != NULL && cJSON_IsNumber(value)) {
      return (float)value->valuedouble;
    }

    return invalidInt;
  }
};
}  // namespace bell