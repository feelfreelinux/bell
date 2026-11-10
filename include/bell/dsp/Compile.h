#pragma once

// Standard includes
#include <cstdint>
#include <memory>
#include <memory_resource>

// Bell includes
#include "bell/dsp/Error.h"
#include "bell/dsp/InternalMemoryAllocator.h"
#include "bell/dsp/Pipeline.h"
#include "bell/dsp/Spec.h"

namespace bell::dsp {

// What to do about the gain a pipeline adds
enum class Headroom : uint8_t {
  // Leave the level alone and report the peak gain, peaks above full scale
  // clip at the output
  Report,
  // Attenuate at the input by the peak gain, PipelineInfo::trimDb says by how
  // much so the caller can add it back downstream
  Trim,
};

struct CompileOptions {
  Headroom headroom = Headroom::Report;

  // Extra attenuation when trimming, the peak gain is exact for sines only
  float headroomMarginDb = 0.0F;

  // Which kernel filter sections run on
  Arithmetic arithmetic = defaultArithmetic();

  // Where coefficients and filter history live, internal RAM on the ESP32
  std::pmr::memory_resource* memory = getInternalMemoryResource();

  // Where long delay lines live, PSRAM on the ESP32
  std::pmr::memory_resource* bulkMemory = std::pmr::get_default_resource();

  // Delay lines stay in `memory` while those of one pipeline fit in this many
  // bytes, so short ones never send the audio thread to slow memory
  size_t internalDelayBytes = 4096;
};

/**
 * @brief Turn a spec into a runnable pipeline for one stream
 *
 * Each channel's gains, filter sections and delay are collected and emitted
 * as one cascade when a mixer reads the channel or the spec ends. Call it
 * away from the audio thread, it designs filters and allocates.
 */
Expected<std::unique_ptr<Pipeline>> compile(const PipelineSpec& spec,
                                            const StreamShape& shape,
                                            const CompileOptions& options = {});

}  // namespace bell::dsp
