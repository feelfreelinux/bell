#include "bell/dsp/Pcm.h"

// Standard includes
#include <cstdint>

using namespace bell::dsp;
using bell::audio::SampleFormat;

namespace {
template <typename T>
bool isAligned(const std::byte* pointer) {
  return reinterpret_cast<uintptr_t>(pointer) % alignof(T) == 0;
}

// Ensure we use the built-in memcpy and don't call the library
template <typename T, bool ALIGNED>
T load(const std::byte* pointer) {
  T value;
  if constexpr (ALIGNED) {
    __builtin_memcpy(&value, __builtin_assume_aligned(pointer, alignof(T)),
                     sizeof(T));
  } else {
    __builtin_memcpy(&value, pointer, sizeof(T));
  }
  return value;
}

template <typename T, bool ALIGNED>
void store(std::byte* pointer, T value) {
  if constexpr (ALIGNED) {
    __builtin_memcpy(__builtin_assume_aligned(pointer, alignof(T)), &value,
                     sizeof(T));
  } else {
    __builtin_memcpy(pointer, &value, sizeof(T));
  }
}

// One channel at a time, stepping over the other channels' samples
template <typename T, bool ALIGNED, typename Convert>
void readChannels(const std::byte* input, Block& block, Convert convert) {
  const size_t stride = block.numChannels * sizeof(T);
  for (size_t channel = 0; channel < block.numChannels; channel++) {
    const std::byte* source = input + channel * sizeof(T);
    for (Sample& sample : block.channel(channel)) {
      sample = convert(load<T, ALIGNED>(source));
      source += stride;
    }
  }
}

template <typename T, bool ALIGNED, typename Convert>
void writeChannels(const Block& block, std::byte* output, Convert convert) {
  const size_t stride = block.numChannels * sizeof(T);
  for (size_t channel = 0; channel < block.numChannels; channel++) {
    std::byte* target = output + channel * sizeof(T);
    for (const Sample& sample : block.channel(channel)) {
      store<T, ALIGNED>(target, convert(sample));
      target += stride;
    }
  }
}

constexpr size_t S24_BYTES = 3;

void readS24(const std::byte* input, Block& block) {
  const size_t stride = block.numChannels * S24_BYTES;
  for (size_t channel = 0; channel < block.numChannels; channel++) {
    const std::byte* source = input + channel * S24_BYTES;
    for (Sample& sample : block.channel(channel)) {
      const int32_t value =
          static_cast<int32_t>(std::to_integer<uint32_t>(source[0]) |
                               (std::to_integer<uint32_t>(source[1]) << 8) |
                               (std::to_integer<uint32_t>(source[2]) << 16));
      // Sign extend from bit 23
      sample = fixed::fromS24((value ^ 0x800000) - 0x800000);
      source += stride;
    }
  }
}

void writeS24(const Block& block, std::byte* output) {
  const size_t stride = block.numChannels * S24_BYTES;
  for (size_t channel = 0; channel < block.numChannels; channel++) {
    std::byte* target = output + channel * S24_BYTES;
    for (const Sample& sample : block.channel(channel)) {
      const auto value = static_cast<uint32_t>(fixed::toS24SatRound(sample));
      target[0] = static_cast<std::byte>(value & 0xFF);
      target[1] = static_cast<std::byte>((value >> 8) & 0xFF);
      target[2] = static_cast<std::byte>((value >> 16) & 0xFF);
      target += stride;
    }
  }
}
}  // namespace

bool pcm::supported(SampleFormat format) {
  return format == SampleFormat::S16 || format == SampleFormat::S24 ||
         format == SampleFormat::S32;
}

void pcm::read(const std::byte* input, SampleFormat format,
               Block& block) noexcept {
  switch (format) {
    case SampleFormat::S16:
      if (isAligned<int16_t>(input)) {
        readChannels<int16_t, true>(input, block, fixed::fromS16);
      } else {
        readChannels<int16_t, false>(input, block, fixed::fromS16);
      }
      break;
    case SampleFormat::S32:
      if (isAligned<int32_t>(input)) {
        readChannels<int32_t, true>(input, block, fixed::fromS32);
      } else {
        readChannels<int32_t, false>(input, block, fixed::fromS32);
      }
      break;
    case SampleFormat::S24:
      readS24(input, block);
      break;
    default:
      break;
  }
}

void pcm::write(const Block& block, SampleFormat format,
                std::byte* output) noexcept {
  switch (format) {
    case SampleFormat::S16:
      if (isAligned<int16_t>(output)) {
        writeChannels<int16_t, true>(block, output, fixed::toS16SatRound);
      } else {
        writeChannels<int16_t, false>(block, output, fixed::toS16SatRound);
      }
      break;
    case SampleFormat::S32:
      if (isAligned<int32_t>(output)) {
        writeChannels<int32_t, true>(block, output, fixed::toS32Sat);
      } else {
        writeChannels<int32_t, false>(block, output, fixed::toS32Sat);
      }
      break;
    case SampleFormat::S24:
      writeS24(block, output);
      break;
    default:
      break;
  }
}
