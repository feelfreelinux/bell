#pragma once

// Bell includes
#include "bell/dsp/Block.h"
#include "bell/dsp/Pipeline.h"

namespace bell::dsp {

/**
 * @brief Multiplies its channels by a constant, saturating
 *
 * Only there for a gain with no filter to be folded into.
 */
class GainNode final : public Node {
 public:
  // Largest factor the node can apply, about +42 dB
  static constexpr double MAX_FACTOR = 127.0;

  // `factor` is linear, negative inverts polarity
  GainNode(ChannelMask channels, double factor);

  void process(Block& block, const Context& context) noexcept override;

 private:
  ChannelMask channels;
  fixed::Gain factor;
};

}  // namespace bell::dsp
