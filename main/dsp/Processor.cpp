#include "bell/dsp/Processor.h"

// Standard includes
#include <algorithm>

// Bell includes
#include "bell/dsp/Pcm.h"

using namespace bell::dsp;

namespace {
// Fade position as a gain, exact as FADE_FRAMES is a power of two
using FadeGain = bell::dsp::fixed::Fixed<30>;
}  // namespace

Processor::Processor(CompileOptions options) : options(options) {}

Processor::~Processor() {
  delete pending.exchange(nullptr);
  delete next;
  delete active;
}

Expected<PipelineInfo> Processor::configure(const StreamShape& shape) {
  std::scoped_lock lock(controlMutex);

  if (!pcm::supported(shape.inputFormat) ||
      !pcm::supported(shape.outputFormat)) {
    return fail("sample format must be 16, 24 or 32 bit signed PCM");
  }

  auto installed = install(spec, shape);
  if (!installed) {
    // The stream has changed regardless, so keep the audio flowing in the
    // new shape by passing it through
    if (auto passthrough = install(PipelineSpec{}, shape); !passthrough) {
      return passthrough;
    }
  }
  stream = shape;
  return installed;
}

Expected<PipelineInfo> Processor::setSpec(PipelineSpec candidate) {
  std::scoped_lock lock(controlMutex);

  if (!stream) {
    spec = std::move(candidate);
    return PipelineInfo{};
  }

  auto installed = install(candidate, *stream);
  if (installed) {
    spec = std::move(candidate);
  }
  return installed;
}

Expected<PipelineInfo> Processor::install(const PipelineSpec& candidate,
                                          const StreamShape& shape) {
  auto pipeline = compile(candidate, shape, options);
  if (!pipeline) {
    return nonstd::make_unexpected(pipeline.error());
  }
  PipelineInfo info = (*pipeline)->info();

  // Replaces a program the audio side has not picked up yet, if there is one
  delete pending.exchange(new Program(std::move(*pipeline), options.memory));
  return info;
}

bell::Result<size_t> Processor::process(std::span<const std::byte> input,
                                        std::span<std::byte> output) noexcept {
  if (Program* incoming = pending.exchange(nullptr)) {
    delete next;
    next = nullptr;
    if (active != nullptr &&
        active->pipeline->shape() == incoming->pipeline->shape()) {
      // Same stream, let the running pipeline fade out first
      next = incoming;
    } else {
      delete active;
      active = incoming;
      fadeLevel = 0;
    }
  }

  if (active == nullptr) {
    return nonstd::make_unexpected(make_error_code(audio::Errc::InvalidFormat));
  }

  // By value, the active program can be replaced part way through the call
  const StreamShape shape = active->pipeline->shape();
  const size_t inputFrameBytes =
      audio::getSampleSizeInBytes(shape.inputFormat) * shape.channels;
  const size_t outputFrameBytes =
      audio::getSampleSizeInBytes(shape.outputFormat) * shape.channels;

  const size_t totalFrames = input.size() / inputFrameBytes;
  if (output.size() < totalFrames * outputFrameBytes) {
    return nonstd::make_unexpected(
        make_error_code(audio::Errc::NotEnoughBytes));
  }

  const Context context{.volumeDb = volume.load(std::memory_order_relaxed)};

  for (size_t done = 0; done < totalFrames;) {
    const size_t frames = std::min(BLOCK_FRAMES, totalFrames - done);
    Block block = active->storage.block(frames);

    pcm::read(input.data() + done * inputFrameBytes, shape.inputFormat, block);
    active->pipeline->process(block, context);

    if (next != nullptr || fadeLevel < FADE_FRAMES) {
      applyFade(block);
    }

    pcm::write(block, shape.outputFormat,
               output.data() + done * outputFrameBytes);
    done += frames;

    if (next != nullptr && fadeLevel == 0) {
      // Faded out, the new pipeline takes over from the next block. It is
      // for the same stream, so the loop carries on
      delete active;
      active = next;
      next = nullptr;
    }
  }

  return totalFrames * outputFrameBytes;
}

void Processor::reset() noexcept {
  if (next != nullptr) {
    // No point finishing a fade out across a gap
    delete active;
    active = next;
    next = nullptr;
  }
  if (active != nullptr) {
    active->pipeline->reset();
  }
  fadeLevel = 0;
}

void Processor::applyFade(Block& block) noexcept {
  const bool fadingOut = next != nullptr;
  const size_t startLevel = fadeLevel;

  for (size_t channel = 0; channel < block.numChannels; channel++) {
    size_t level = startLevel;
    for (Sample& sample : block.channel(channel)) {
      if (fadingOut) {
        level -= level > 0 ? 1 : 0;
      } else {
        level += level < FADE_FRAMES ? 1 : 0;
      }
      const FadeGain gain = FadeGain::fromRaw(
          static_cast<int32_t>(level * (size_t{1} << 30) / FADE_FRAMES));
      sample = fixed::mulTo<Sample::FRAC>(sample, gain);
    }
    fadeLevel = level;
  }
}
