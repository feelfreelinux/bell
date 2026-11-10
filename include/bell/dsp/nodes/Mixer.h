#pragma once

// Standard includes
#include <memory_resource>
#include <span>
#include <vector>

// Bell includes
#include "bell/dsp/Block.h"
#include "bell/dsp/Pipeline.h"

namespace bell::dsp {

// One weighted source of a route, the factor is linear and may be negative
struct MixTap {
  size_t from = 0;
  double factor = 1.0;
};

// One output channel as a weighted sum of input channels
struct MixRoute {
  size_t to = 0;
  std::vector<MixTap> taps;
};

/**
 * @brief Rebuilds some channels as weighted sums of channels
 *
 * Every route reads the channels as they were on entry, so routes can swap
 * channels. A sum saturates once, at the end.
 */
class MixerNode final : public Node {
 public:
  // Largest weight a tap can carry, +24 dB. With MAX_CHANNELS taps at most
  // the 64-bit sum cannot overflow
  static constexpr double MAX_FACTOR = 16.0;

  // Each route has at most MAX_CHANNELS taps, none above MAX_FACTOR
  MixerNode(std::span<const MixRoute> routes,
            std::pmr::memory_resource* memory);

  void process(Block& block, const Context& context) noexcept override;

 private:
  struct Tap {
    size_t from;
    fixed::Gain factor;
  };

  struct Route {
    size_t to;
    size_t firstTap;
    size_t tapCount;
  };

  void mix(const Route& route, const Block& block,
           Sample* output) const noexcept;

  std::pmr::vector<Route> routes;
  std::pmr::vector<Tap> taps;
  // One block per route, all are computed before any is written back
  std::pmr::vector<Sample> scratch;
};

}  // namespace bell::dsp
