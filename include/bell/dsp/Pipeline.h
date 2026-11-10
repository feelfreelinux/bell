#pragma once

// Standard includes
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Bell includes
#include "bell/audio/Common.h"
#include "bell/dsp/Block.h"

namespace bell::dsp {

/**
 * @brief The stream a pipeline is built for, a change to it is a recompile
 */
struct StreamShape {
  uint32_t sampleRate = 48000;
  uint8_t channels = 2;
  // Independent, so 16-bit input can leave as 32-bit without losing bits
  audio::SampleFormat inputFormat = audio::SampleFormat::S16;
  audio::SampleFormat outputFormat = audio::SampleFormat::S16;

  bool operator==(const StreamShape&) const = default;
};

/**
 * @brief The kernel biquad sections run on, the rest is fixed point either way
 */
enum class Arithmetic : uint8_t {
  // Direct form 1, 64-bit accumulator with error feedback. Needs no FPU
  Fixed,
  // State-variable filter in single precision, the fast one on an FPU
  Float,
};

// Float when built with BELL_DSP_FLOAT_FILTERS, otherwise Fixed
Arithmetic defaultArithmetic();

/**
 * @brief Values that change while a pipeline runs, read once per block
 */
struct Context {
  // Playback volume applied downstream of the DSP, for nodes that depend on it
  float volumeDb = 0.0F;
};

/**
 * @brief One processing step of a compiled pipeline
 *
 * process() runs on the audio thread, so no allocation, locks or exceptions.
 * It touches only its own channels and takes up to BLOCK_FRAMES frames.
 */
class Node {
 public:
  virtual ~Node() = default;

  virtual void process(Block& block, const Context& context) noexcept = 0;

  // Forget all history, as after a gap in the stream
  virtual void reset() noexcept {}
};

/**
 * @brief What compile() made of a spec
 */
struct PipelineInfo {
  // Largest gain any channel applies to a sine, before trim: the headroom
  // the pipeline needs
  float peakGainDb = 0.0F;

  // Attenuation applied at the input for Headroom::Trim, zero or negative
  float trimDb = 0.0F;

  // Biquad sections run per frame, summed over channels
  size_t sections = 0;

  // The kernel those sections run on
  Arithmetic arithmetic = Arithmetic::Fixed;

  // One line per emitted node, naming the spec nodes it came from
  std::vector<std::string> nodes;

  std::string describe() const;
};

/**
 * @brief A compiled, runnable pipeline. Only filter and delay history change
 */
class Pipeline {
 public:
  Pipeline(StreamShape shape, std::vector<std::unique_ptr<Node>> nodes,
           PipelineInfo info)
      : streamShape(shape),
        nodes(std::move(nodes)),
        pipelineInfo(std::move(info)) {}

  // Runs every node over the block, in place
  void process(Block& block, const Context& context) noexcept {
    for (auto& node : nodes) {
      node->process(block, context);
    }
  }

  void reset() noexcept {
    for (auto& node : nodes) {
      node->reset();
    }
  }

  const StreamShape& shape() const { return streamShape; }
  const PipelineInfo& info() const { return pipelineInfo; }

 private:
  StreamShape streamShape;
  std::vector<std::unique_ptr<Node>> nodes;
  PipelineInfo pipelineInfo;
};

}  // namespace bell::dsp
