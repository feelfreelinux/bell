#pragma once

// Standard includes
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory_resource>
#include <span>
#include <vector>

// Bell includes
#include "bell/dsp/Fixed.h"
#include "bell/dsp/InternalMemoryAllocator.h"

namespace bell::dsp {

using fixed::Sample;

// Most channels a stream can have, also the width of ChannelMask
inline constexpr size_t MAX_CHANNELS = 8;

// Most frames a node gets in one call, the processor cuts its input to this
inline constexpr size_t BLOCK_FRAMES = 128;

/**
 * @brief A set of channel indices
 */
class ChannelMask {
 public:
  constexpr ChannelMask() = default;

  constexpr ChannelMask(std::initializer_list<size_t> channels) {
    for (size_t channel : channels) {
      add(channel);
    }
  }

  // Channels 0 .. count-1
  static constexpr ChannelMask first(size_t count) {
    ChannelMask mask;
    for (size_t channel = 0; channel < count && channel < MAX_CHANNELS;
         channel++) {
      mask.add(channel);
    }
    return mask;
  }

  // An index at or above MAX_CHANNELS is ignored, check before adding
  constexpr void add(size_t channel) {
    if (channel < MAX_CHANNELS) {
      bits |= static_cast<uint8_t>(1U << channel);
    }
  }

  constexpr bool contains(size_t channel) const {
    return channel < MAX_CHANNELS && ((bits >> channel) & 1U) != 0;
  }

  constexpr bool empty() const { return bits == 0; }

  constexpr size_t count() const {
    size_t total = 0;
    for (size_t channel = 0; channel < MAX_CHANNELS; channel++) {
      total += contains(channel) ? 1 : 0;
    }
    return total;
  }

  // True if every channel in the set exists in a stream of numChannels
  constexpr bool within(size_t numChannels) const {
    return (bits & ~first(numChannels).bits) == 0;
  }

  constexpr bool operator==(const ChannelMask&) const = default;

 private:
  uint8_t bits = 0;
};

/**
 * @brief A view of planar audio, one run of samples per channel. Owns nothing
 */
struct Block {
  std::array<Sample*, MAX_CHANNELS> data{};
  size_t numChannels = 0;
  size_t frames = 0;

  std::span<Sample> channel(size_t index) const {
    return {data[index], frames};
  }
};

/**
 * @brief Owns the samples behind a Block, in internal RAM on the ESP32
 */
class BlockStorage {
 public:
  explicit BlockStorage(
      size_t numChannels, size_t maxFrames = BLOCK_FRAMES,
      std::pmr::memory_resource* memory = getInternalMemoryResource())
      : samples(numChannels * maxFrames, Sample::zero(), memory),
        numChannels(numChannels),
        maxFrames(maxFrames) {}

  // A block over the first `frames` frames of every channel
  Block block(size_t frames) {
    Block view;
    view.numChannels = numChannels;
    view.frames = frames < maxFrames ? frames : maxFrames;
    for (size_t channel = 0; channel < numChannels; channel++) {
      view.data[channel] = samples.data() + channel * maxFrames;
    }
    return view;
  }

 private:
  std::pmr::vector<Sample> samples;
  size_t numChannels;
  size_t maxFrames;
};

}  // namespace bell::dsp
