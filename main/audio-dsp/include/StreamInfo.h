#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

namespace bell {
enum class Channels { LEFT, RIGHT, LEFT_RIGHT };

enum class SampleRate : uint32_t {
  SR_44100 = 44100,
  SR_48000 = 48000,
};

enum class BitWidth : uint32_t {
  BW_16 = 16,
  BW_24 = 24,
  BW_32 = 32,
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