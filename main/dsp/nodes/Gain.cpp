#include "bell/dsp/nodes/Gain.h"

using namespace bell::dsp;

GainNode::GainNode(ChannelMask channels, double factor)
    : channels(channels), factor(fixed::Gain::fromDouble(factor)) {}

void GainNode::process(Block& block, const Context& /*context*/) noexcept {
  const fixed::Gain gain = factor;
  for (size_t channel = 0; channel < block.numChannels; channel++) {
    if (!channels.contains(channel)) {
      continue;
    }
    for (Sample& sample : block.channel(channel)) {
      sample = fixed::mulToSat<Sample::FRAC>(sample, gain);
    }
  }
}
