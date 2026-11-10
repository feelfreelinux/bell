#pragma once

// Standard includes
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <span>

// Bell includes
#include "bell/Result.h"
#include "bell/dsp/Compile.h"
#include "bell/dsp/Pipeline.h"
#include "bell/dsp/Spec.h"

namespace bell::dsp {

/**
 * @brief Runs a pipeline over interleaved PCM
 *
 * configure(), setSpec() and setVolumeDb() are the control side, process() and
 * reset() the audio side, which never waits on the other. A new pipeline is
 * taken at the next process() call, with a short fade out and back in.
 */
class Processor {
 public:
  explicit Processor(CompileOptions options = {});
  ~Processor();

  Processor(const Processor&) = delete;
  Processor& operator=(const Processor&) = delete;

  /**
   * @brief Describe the stream process() will be given, compiling the spec
   *
   * A spec that does not suit the stream leaves the audio passing through and
   * returns the error. An unsupported sample format changes nothing.
   */
  Expected<PipelineInfo> configure(const StreamShape& shape);

  /**
   * @brief Replace the pipeline description
   *
   * On error the running pipeline and the previous spec are kept. Before the
   * first configure() the spec is only stored.
   */
  Expected<PipelineInfo> setSpec(PipelineSpec candidate);

  // Playback volume, handed to every node in its Context. No node reads it yet
  void setVolumeDb(float volumeDb) noexcept {
    volume.store(volumeDb, std::memory_order_relaxed);
  }

  /**
   * @brief Process interleaved frames from `input` into `output`
   *
   * Both may be the same buffer if an output sample is no wider than an input
   * one. Trailing bytes short of a whole frame are ignored.
   *
   * @return bytes written, or an error with no stream configured or an
   *         `output` that is too small
   */
  bell::Result<size_t> process(std::span<const std::byte> input,
                               std::span<std::byte> output) noexcept;

  /**
   * @brief Forget filter and delay history, as after a gap in the stream
   *
   * Audio side, call it from the thread that calls process(). The next block
   * fades in.
   */
  void reset() noexcept;

 private:
  // A pipeline with the block memory it runs on
  struct Program {
    Program(std::unique_ptr<Pipeline> pipeline,
            std::pmr::memory_resource* memory)
        : pipeline(std::move(pipeline)),
          storage(this->pipeline->shape().channels, BLOCK_FRAMES, memory) {}

    std::unique_ptr<Pipeline> pipeline;
    BlockStorage storage;
  };

  // Length of the fade out and of the fade in around a pipeline change
  static constexpr size_t FADE_FRAMES = BLOCK_FRAMES;

  Expected<PipelineInfo> install(const PipelineSpec& candidate,
                                 const StreamShape& shape);
  void applyFade(Block& block) noexcept;

  // Control side
  CompileOptions options;
  std::mutex controlMutex;
  PipelineSpec spec;
  std::optional<StreamShape> stream;

  // Handed from the control side to the audio side
  std::atomic<Program*> pending{nullptr};
  std::atomic<float> volume{0.0F};

  // Audio side
  Program* active = nullptr;
  Program* next = nullptr;  // waiting for the active one to fade out
  size_t fadeLevel = 0;     // 0 is silent, FADE_FRAMES is full level
};

}  // namespace bell::dsp
