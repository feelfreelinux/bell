#include "bell/dsp/nodes/Delay.h"

// Standard includes
#include <algorithm>

using namespace bell::dsp;

DelayNode::DelayNode(ChannelMask channels, size_t frames,
                     std::pmr::memory_resource* memory)
    : channels(channels),
      frames(std::max<size_t>(frames, 1)),
      lines(channels.count() * this->frames, Sample::zero(), memory) {}

void DelayNode::reset() noexcept {
  std::fill(lines.begin(), lines.end(), Sample::zero());
  position = 0;
}

void DelayNode::process(Block& block, const Context& /*context*/) noexcept {
  size_t slot = 0;
  size_t next = position;
  for (size_t channel = 0; channel < block.numChannels; channel++) {
    if (!channels.contains(channel)) {
      continue;
    }
    Sample* line = &lines[slot * frames];
    // Every channel starts from the same place in its ring
    next = position;
    for (Sample& sample : block.channel(channel)) {
      // The slot about to be overwritten holds the sample from `frames` ago
      std::swap(sample, line[next]);
      if (++next == frames) {
        next = 0;
      }
    }
    slot++;
  }
  position = next;
}
