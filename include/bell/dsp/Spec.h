#pragma once

// Standard includes
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Bell includes
#include "bell/dsp/Block.h"

/**
 * @file Spec.h
 * @brief What a pipeline should do, as plain data
 *
 * compile() turns a spec into a runnable Pipeline for a given stream.
 * Frequencies are in Hz and gains in dB throughout.
 */

namespace bell::dsp::spec {

// How wide a second-order filter is. Each filter lists the forms it takes

struct Q {
  float value = 0.70710678F;
  bool operator==(const Q&) const = default;
};

struct Bandwidth {
  float octaves = 1.0F;
  bool operator==(const Bandwidth&) const = default;
};

// Shelf steepness, 12 dB per octave is the steepest that stays monotonic
struct Slope {
  float dbPerOctave = 12.0F;
  bool operator==(const Slope&) const = default;
};

// How the sections of a higher-order low or high pass are tuned
enum class Alignment : uint8_t {
  Butterworth,    // maximally flat, -3 dB at the corner
  LinkwitzRiley,  // -6 dB at the corner; a low/high pair sums flat
};

// Nodes

struct Gain {
  float gainDb = 0.0F;
  bool invert = false;
  bool operator==(const Gain&) const = default;
};

struct Peaking {
  float freqHz = 1000.0F;
  float gainDb = 0.0F;
  std::variant<Q, Bandwidth> width = Q{};
  bool operator==(const Peaking&) const = default;
};

// order is 1 or 2, a first-order shelf has no width
struct LowShelf {
  float freqHz = 100.0F;
  float gainDb = 0.0F;
  uint8_t order = 2;
  std::variant<Q, Slope> width = Q{};
  bool operator==(const LowShelf&) const = default;
};

struct HighShelf {
  float freqHz = 8000.0F;
  float gainDb = 0.0F;
  uint8_t order = 2;
  std::variant<Q, Slope> width = Q{};
  bool operator==(const HighShelf&) const = default;
};

// order is 1 to 8, an explicit Q only applies to order 2. A crossover is a
// LowPass and a HighPass with the same frequency, order and alignment
struct LowPass {
  float freqHz = 1000.0F;
  uint8_t order = 2;
  std::variant<Alignment, Q> shape = Alignment::Butterworth;
  bool operator==(const LowPass&) const = default;
};

struct HighPass {
  float freqHz = 1000.0F;
  uint8_t order = 2;
  std::variant<Alignment, Q> shape = Alignment::Butterworth;
  bool operator==(const HighPass&) const = default;
};

// Unity gain at the centre frequency
struct BandPass {
  float freqHz = 1000.0F;
  std::variant<Q, Bandwidth> width = Q{};
  bool operator==(const BandPass&) const = default;
};

struct Notch {
  float freqHz = 1000.0F;
  std::variant<Q, Bandwidth> width = Q{};
  bool operator==(const Notch&) const = default;
};

// order is 1 or 2, a first-order all pass has no width
struct AllPass {
  float freqHz = 1000.0F;
  uint8_t order = 2;
  std::variant<Q, Bandwidth> width = Q{};
  bool operator==(const AllPass&) const = default;
};

// H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2), used as given
// whatever the sample rate
struct Biquad {
  double b0 = 1.0;
  double b1 = 0.0;
  double b2 = 0.0;
  double a1 = 0.0;
  double a2 = 0.0;
  bool operator==(const Biquad&) const = default;
};

// One source of a mixer route
struct MixerInput {
  uint8_t channel = 0;
  float gainDb = 0.0F;
  bool invert = false;
  bool operator==(const MixerInput&) const = default;
};

// What one output channel is made of, no inputs is silence
struct MixerRoute {
  uint8_t to = 0;
  std::vector<MixerInput> from;
  bool operator==(const MixerRoute&) const = default;
};

/**
 * @brief Rebuilds channels as weighted sums of channels
 *
 * Every route reads the channels as they were before the mixer, and a channel
 * without a route passes through. NodeSpec::channels stays empty.
 */
struct Mixer {
  std::vector<MixerRoute> routes;
  bool operator==(const Mixer&) const = default;
};

struct Milliseconds {
  float value = 0.0F;
  bool operator==(const Milliseconds&) const = default;
};

struct Samples {
  uint32_t count = 0;
  bool operator==(const Samples&) const = default;
};

// Holds its channels back, by up to a second. A time is rounded to whole
// samples of the stream, a count is exact but depends on the sample rate
struct Delay {
  std::variant<Milliseconds, Samples> amount = Milliseconds{};
  bool operator==(const Delay&) const = default;
};

}  // namespace bell::dsp::spec

namespace bell::dsp {

using NodeParams =
    std::variant<spec::Gain, spec::Peaking, spec::LowShelf, spec::HighShelf,
                 spec::LowPass, spec::HighPass, spec::BandPass, spec::Notch,
                 spec::AllPass, spec::Biquad, spec::Mixer, spec::Delay>;

struct NodeSpec {
  NodeParams params;

  // Channels the node applies to, unset is every channel of the stream
  std::optional<ChannelMask> channels;

  // A disabled node is skipped, as if it were not in the list
  bool enabled = true;

  // Free-form name, for tools
  std::string id;

  bool operator==(const NodeSpec&) const = default;
};

// Nodes run in order
struct PipelineSpec {
  std::vector<NodeSpec> nodes;
  bool operator==(const PipelineSpec&) const = default;
};

}  // namespace bell::dsp
