#pragma once

// Standard includes
#include <memory_resource>
#include <vector>

// Bell includes
#include "bell/dsp/Block.h"
#include "bell/dsp/Pipeline.h"

namespace bell::dsp {

/**
 * @brief Holds its channels back by a whole number of frames, silence first
 */
class DelayNode final : public Node {
 public:
  // `frames` is at least 1
  DelayNode(ChannelMask channels, size_t frames,
            std::pmr::memory_resource* memory);

  void process(Block& block, const Context& context) noexcept override;
  void reset() noexcept override;

 private:
  ChannelMask channels;
  size_t frames;
  // One ring of `frames` samples per channel, all at the same position
  std::pmr::vector<Sample> lines;
  size_t position = 0;
};

}  // namespace bell::dsp
