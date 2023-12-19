#pragma once

#include <memory>  // for unique_ptr
#include <mutex>   // for scoped_lock
#include <vector>  // for vector

#include "AudioTransform.h"   // for AudioTransform
#include "StreamInfo.h"       // for StreamInfo
#include "TransformConfig.h"  // for TransformConfig

struct SpeexResamplerState_;

namespace bell {
class AudioResampler final : public bell::AudioTransform {
 public:
  AudioResampler();
  ~AudioResampler();

  void configure(int channels, SampleRate from, SampleRate target);

  std::unique_ptr<StreamInfo> process(
      std::unique_ptr<StreamInfo> data) override;

  void reconfigure() override { std::scoped_lock lock(this->accessMutex); }

  void sampleRateChanged(uint32_t sampleRate) override{};

 private:
  SpeexResamplerState_* resampler = nullptr;
  SampleRate from, to;
};
}  // namespace bell