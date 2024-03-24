#pragma once

#include <stddef.h>    // for size_t
#include <stdint.h>    // for uint32_t, uint8_t
#include <functional>  // for function
#include <memory>      // for shared_ptr, unique_ptr
#include <mutex>       // for mutex
#include <vector>      // for vector

#include "StreamInfo.h"  // for BitWidth

namespace bell {
class AudioPipeline;
class CentralAudioBuffer;

#define MAX_INT16 32767

class BellDSP {
 public:
  BellDSP();
  ~BellDSP();

  class AudioEffect {
   public:
    AudioEffect() = default;
    virtual ~AudioEffect() = default;
    size_t duration;
    virtual void apply(float* sampleData, size_t samples,
                       size_t relativePosition) = 0;
  };

  class FadeEffect : public AudioEffect {
   private:
    std::function<void()> onFinish;
    bool isFadeIn;

   public:
    FadeEffect(size_t duration, bool isFadeIn,
               std::function<void()> onFinish = nullptr);
    ~FadeEffect(){};

    void apply(float* sampleData, size_t samples, size_t relativePosition);
  };

  void applyPipeline(std::shared_ptr<AudioPipeline> pipeline);

  /**
   * @brief Executes an instant effect on the pipeline data, can be called from other thread
   * 
   * @param instantEffect effect to-be-executed
   */
  void queryInstantEffect(std::unique_ptr<AudioEffect> instantEffect);

  /**
   * @brief Returns the currently active pipeline object
   * 
   * @return std::shared_ptr<AudioPipeline> active pipeline
   */
  std::shared_ptr<AudioPipeline> getActivePipeline();

  /**
   * @brief Executes the provided DSP pipeline, on provided PCM data
   * 
   * @param inputBuffer Pointer to a buffer storing PCM frames to be processed
   * @param inputBufferLen Input buffer length, in bytes
   * @param outputBuffer Pointer to a buffer where processed PCM data will be stored
   * @param outputBufferLen Length of the output buffer. Needs to fit the expected processed PCM output.
   * @param channels Number of channels in the input
   * @param sampleRate Input sample rate
   * @param bitWidth Input bitwidth
   * @return StreamInfo object containing resulting processed data information
   */
  std::unique_ptr<StreamInfo> process(const uint8_t* inputBuffer,
                                      size_t inputBufferLen,
                                      uint8_t* outputBuffer,
                                      size_t outputBufferLen, int channels,
                                      SampleRate sampleRate, BitWidth bitWidth);

 private:
  std::shared_ptr<AudioPipeline> activePipeline;
  std::mutex accessMutex;

  std::shared_ptr<DSPDataSlots> dataSlots;

  std::unique_ptr<AudioEffect> underflowEffect = nullptr;
  std::unique_ptr<AudioEffect> startEffect = nullptr;
  std::unique_ptr<AudioEffect> instantEffect = nullptr;

  size_t samplesSinceInstantQueued;
};
};  // namespace bell
