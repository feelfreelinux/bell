#include "bell/dsp/nodes/Mixer.h"

// Standard includes
#include <algorithm>
#include <cstring>

using namespace bell::dsp;

namespace {
// Sum of sample * weight products, at the precision of both
using MixAcc = fixed::Fixed<Sample::FRAC + fixed::Gain::FRAC, int64_t>;
}  // namespace

MixerNode::MixerNode(std::span<const MixRoute> source,
                     std::pmr::memory_resource* memory)
    : routes(memory), taps(memory), scratch(memory) {
  for (const MixRoute& route : source) {
    routes.push_back({route.to, taps.size(), route.taps.size()});
    for (const MixTap& tap : route.taps) {
      taps.push_back({tap.from, fixed::Gain::fromDouble(tap.factor)});
    }
  }
  scratch.assign(routes.size() * BLOCK_FRAMES, Sample::zero());
}

void MixerNode::process(Block& block, const Context& /*context*/) noexcept {
  const size_t frames = std::min(block.frames, BLOCK_FRAMES);

  // All routes are worked out from the untouched block first
  for (size_t index = 0; index < routes.size(); index++) {
    mix(routes[index], block, &scratch[index * BLOCK_FRAMES]);
  }
  // and only then written over their channels
  for (size_t index = 0; index < routes.size(); index++) {
    std::memcpy(block.data[routes[index].to], &scratch[index * BLOCK_FRAMES],
                frames * sizeof(Sample));
  }
}

void MixerNode::mix(const Route& route, const Block& block,
                    Sample* output) const noexcept {
  using namespace fixed;

  const size_t frames = std::min(block.frames, BLOCK_FRAMES);
  // Not taps[firstTap], a silent route has no taps to index
  const Tap* tap = taps.data() + route.firstTap;

  // Nearly every real mixer is a copy, a swap or a sum of two
  switch (route.tapCount) {
    case 0:
      std::fill_n(output, frames, Sample::zero());
      return;

    case 1: {
      const Sample* source = block.data[tap[0].from];
      const Gain factor = tap[0].factor;
      if (factor == Gain::one()) {
        std::memcpy(output, source, frames * sizeof(Sample));
        return;
      }
      for (size_t i = 0; i < frames; i++) {
        output[i] = mulToSat<Sample::FRAC>(source[i], factor);
      }
      return;
    }

    case 2: {
      const Sample* first = block.data[tap[0].from];
      const Sample* second = block.data[tap[1].from];
      const Gain firstFactor = tap[0].factor;
      const Gain secondFactor = tap[1].factor;
      for (size_t i = 0; i < frames; i++) {
        MixAcc acc = MixAcc::zero();
        acc = mac(acc, first[i], firstFactor);
        acc = mac(acc, second[i], secondFactor);
        output[i] = narrowSat<Sample::FRAC>(acc);
      }
      return;
    }

    default:
      for (size_t i = 0; i < frames; i++) {
        MixAcc acc = MixAcc::zero();
        for (size_t t = 0; t < route.tapCount; t++) {
          acc = mac(acc, block.data[tap[t].from][i], tap[t].factor);
        }
        output[i] = narrowSat<Sample::FRAC>(acc);
      }
      return;
  }
}
