#pragma once

#include <memory>
#include "AudioPipeline.h"
#ifndef BELL_ONLY_CJSON

// dsp filters
#include "AudioMixer.h"
#include "Biquad.h"
#include "BiquadCombo.h"
#include "Compressor.h"
#include "Gain.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include "TransformConfig.h"

namespace bell {
class NlohmannJSONTransformConfig : public bell::TransformConfig {
 private:
  nlohmann::json json;

 public:
  NlohmannJSONTransformConfig(nlohmann::json& body) { this->json = body; };
  ~NlohmannJSONTransformConfig(){};

  static std::shared_ptr<AudioPipeline> parsePipeline(
      nlohmann::json& dspPipeline) {
    auto pipeline = std::make_shared<bell::AudioPipeline>();

    for (auto& transform : dspPipeline) {
      if (!transform.contains("type"))
        continue;

      std::string type = transform["type"];
      if (type == "gain") {
        auto filter = std::make_shared<bell::Gain>();
        filter->config =
            std::make_unique<bell::NlohmannJSONTransformConfig>(transform);
        filter->reconfigure();
        pipeline->addTransform(filter);
        std::cout << "got gain" << std::endl;
      } else if (type == "compressor") {
        auto filter = std::make_shared<bell::Compressor>();
        filter->config =
            std::make_unique<bell::NlohmannJSONTransformConfig>(transform);
        filter->reconfigure();
        pipeline->addTransform(filter);
      } else if (type == "mixer") {
        auto filter = std::make_shared<bell::AudioMixer>();
        filter->config =
            std::make_unique<bell::NlohmannJSONTransformConfig>(transform);
        filter->reconfigure();
        pipeline->addTransform(filter);
      } else if (type == "biquad") {
        auto filter = std::make_shared<bell::Biquad>();
        filter->config =
            std::make_unique<bell::NlohmannJSONTransformConfig>(transform);
        filter->reconfigure();
        pipeline->addTransform(filter);
      } else if (type == "biquad_combo") {
        auto filter = std::make_shared<bell::BiquadCombo>();
        filter->config =
            std::make_unique<bell::NlohmannJSONTransformConfig>(transform);
        filter->reconfigure();
        pipeline->addTransform(filter);
      } else {
        throw std::invalid_argument(
            "No filter type found for configuration field " + type);
      }
    }

    pipeline->volumeUpdated(50);
    return pipeline;
  }

  std::vector<MixerConfig> rawGetMixerConfig(
      const std::string& field) override {
    if (!json.contains(field) || !json[field].is_array()) {
      throw std::invalid_argument("Mixer configuration invalid");
    }

    std::vector<MixerConfig> result;

    for (auto& field : json[field]) {
      std::vector<int> sources = field["source"];
      int destination = field["destination"];
      result.push_back(
          MixerConfig{.source = sources, .destination = destination});
    }

    return result;
  }

  std::string rawGetString(const std::string& field) override {
    if (json.contains(field) && json[field].is_string()) {
      return json[field];
    }

    return "invalid";
  }

  std::vector<int> rawGetIntArray(const std::string& field) override {
    std::vector<int> result;

    if (json.contains(field) && json[field].is_array()) {
      for (auto& field : json[field]) {
        if (!field.is_number_integer()) {
          continue;
        }

        result.push_back(field);
      }
    }

    return result;
  }

  std::vector<float> rawGetFloatArray(const std::string& field) override {
    std::vector<float> result;

    if (json.contains(field) && json[field].is_array()) {
      for (auto& field : json[field]) {
        if (!field.is_number_float() && !field.is_number()) {
          continue;
        }

        result.push_back(field);
      }
    }

    return result;
  }

  int rawGetInt(const std::string& field) override {
    if (json.contains(field) && json[field].is_number_integer()) {
      return json[field];
    }

    return invalidInt;
  }

  bool isArray(const std::string& field) override {
    if (json.contains(field) && json[field].is_array()) {
      return true;
    }

    return false;
  }

  float rawGetFloat(const std::string& field) override {
    if (json.contains(field) &&
        (json[field].is_number_float() || json[field].is_number())) {
      return json[field];
    }

    return invalidInt;
  }
};
}  // namespace bell

#endif