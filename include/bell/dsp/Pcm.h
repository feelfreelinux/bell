#pragma once

// Standard includes
#include <cstddef>

// Bell includes
#include "bell/audio/Common.h"
#include "bell/dsp/Block.h"

/**
 * @file Pcm.h
 * @brief Moving audio between interleaved PCM bytes and planar blocks
 *
 * PCM full scale is 1.0 in a block whatever the width. Output rounds and
 * saturates, so what went past full scale clips instead of wrapping.
 */

namespace bell::dsp::pcm {

// True for what read() and write() handle: S16, packed S24 and S32, little
// endian
bool supported(audio::SampleFormat format);

// Fills `block` from `block.frames` interleaved frames at `input`
void read(const std::byte* input, audio::SampleFormat format,
          Block& block) noexcept;

// Writes `block.frames` interleaved frames to `output`
void write(const Block& block, audio::SampleFormat format,
           std::byte* output) noexcept;

}  // namespace bell::dsp::pcm
