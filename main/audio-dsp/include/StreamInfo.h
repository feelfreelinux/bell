#pragma once

#include <array>
#include <memory>
#include <unordered_map>

namespace bell {
enum class SampleRate : uint32_t {
  SR_44100 = 44100,
  SR_48000 = 48000,
};

enum class BitWidth : uint32_t {
  BW_16 = 16,
  BW_24 = 24,
  BW_32 = 32,
};

/**
 * @brief Simple helper over essential pcm audio stream params (bitwidth, sample rate, channels)
 */
struct AudioParams {
  BitWidth bitwidth;
  SampleRate sampleRate;
  int channels;

  AudioParams() {
    // Roughly default params
    bitwidth = BitWidth::BW_16;
    sampleRate = SampleRate::SR_44100;
    channels = 2;
  }

  AudioParams(BitWidth bw, SampleRate sr, int channels) {
    this->bitwidth = bw;
    this->sampleRate = sr;
    this->channels = channels;
  }

  /**
   * @brief Get the amount of bytes needed to store audio of given length
   * 
   * @param milliseconds milliseconds of audio stream
   * @return bytes necessary to store the audio fragment 
   */
  inline int64_t getBytesPerDuration(int64_t milliseconds) {
    return getBytesPerFrames(millisecondsToFrames(milliseconds));
  }

  /**
   * @brief Converts milliseconds to audio frames at given parameters
   * 
   * @param milliseconds milliseconds of audio stream
   * @return frames in given timespan
   */
  inline int64_t millisecondsToFrames(int64_t milliseconds) {
    return static_cast<uint32_t>(sampleRate) * milliseconds / 1000;
  }

  /**
   * @brief Get the amount of bytes needed to store audio of given length
   * 
   * @param frames frames of audio stream
   * @return bytes necessary to store the audio fragment
   */
  inline int64_t getBytesPerFrames(int64_t frames) {
    return channels * (static_cast<int>(bitwidth) / 8) * frames;
  }

  bool operator!=(const AudioParams& other) const {
    return (bitwidth != other.bitwidth) || (sampleRate != other.sampleRate) ||
           (channels != other.channels);
  }
};

static const int dspMaxFrames = 1024 * 2;
static const int dspMaxChannels = 2;

using DSPDataSlots = std::unordered_map<int, std::array<float, 2048>>;

typedef struct {
  std::shared_ptr<DSPDataSlots> data;
  BitWidth bitwidth;
  int numChannels;
  SampleRate sampleRate;
  size_t numSamples;
} StreamInfo;
};  // namespace bell