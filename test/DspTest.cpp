#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "bell/dsp/Compile.h"
#include "bell/dsp/Processor.h"
#include "bell/dsp/nodes/Biquad.h"

#ifndef BELL_DISABLE_TAOJSON
// The library's own headers do not build cleanly under -Wextra
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <tao/json.hpp>
#pragma GCC diagnostic pop

#include "bell/dsp/TaoJson.h"
#endif

using namespace bell::dsp;

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double SAMPLE_RATE = 48000.0;

using Channels = std::vector<std::vector<Sample>>;

NodeSpec node(NodeParams params) {
  return NodeSpec{.params = std::move(params)};
}

NodeSpec node(NodeParams params, ChannelMask channels) {
  return NodeSpec{.params = std::move(params), .channels = channels};
}

std::unique_ptr<Pipeline> build(const PipelineSpec& spec,
                                Arithmetic arithmetic = defaultArithmetic()) {
  CompileOptions options;
  options.arithmetic = arithmetic;
  auto pipeline =
      compile(spec, StreamShape{.sampleRate = 48000, .channels = 2}, options);
  REQUIRE_MESSAGE(pipeline, pipeline.error().describe());
  return std::move(*pipeline);
}

// Runs planar samples through a pipeline, a block at a time
void run(Pipeline& pipeline, Channels& channels) {
  const size_t total = channels.front().size();
  for (size_t done = 0; done < total; done += BLOCK_FRAMES) {
    Block block;
    block.numChannels = channels.size();
    block.frames = std::min(BLOCK_FRAMES, total - done);
    for (size_t channel = 0; channel < channels.size(); channel++) {
      block.data[channel] = channels[channel].data() + done;
    }
    pipeline.process(block, Context{});
  }
}

// A ramp that is different on each channel
Channels ramp(size_t frames) {
  Channels channels(2, std::vector<Sample>(frames));
  for (size_t i = 0; i < frames; i++) {
    channels[0][i] = Sample::fromDouble(0.001 * static_cast<double>(i % 500));
    channels[1][i] = Sample::fromDouble(-0.002 * static_cast<double>(i % 200));
  }
  return channels;
}

// Gain in dB that a pipeline applies to a sine on channel 0, once settled
double gainDbAt(Pipeline& pipeline, double freqHz) {
  const size_t frames = 48000;
  Channels channels(2, std::vector<Sample>(frames));
  for (size_t i = 0; i < frames; i++) {
    const double phase =
        2.0 * PI * freqHz * static_cast<double>(i) / SAMPLE_RATE;
    channels[0][i] = channels[1][i] =
        Sample::fromDouble(0.125 * std::sin(phase));
  }
  run(pipeline, channels);

  double power = 0.0;
  for (size_t i = frames / 2; i < frames; i++) {
    power += channels[0][i].toDouble() * channels[0][i].toDouble();
  }
  const double rms = std::sqrt(power / static_cast<double>(frames / 2));
  return 20.0 * std::log10(rms / (0.125 / std::sqrt(2.0)));
}

double designedGainDb(const std::vector<Section>& sections, double freqHz) {
  std::complex<double> response = 1.0;
  for (const Section& section : sections) {
    response *= section.response(2.0 * PI * freqHz / SAMPLE_RATE);
  }
  return 20.0 * std::log10(std::abs(response));
}
}  // namespace

TEST_CASE("An empty pipeline changes nothing") {
  const auto pipeline = build(PipelineSpec{});
  Channels channels = ramp(1000);
  const Channels original = channels;
  run(*pipeline, channels);
  CHECK(channels == original);
}

TEST_CASE("Filters match their design on both kernels") {
  const spec::Peaking peaking{
      .freqHz = 1000, .gainDb = 6, .width = spec::Q{1.0F}};
  const spec::HighPass highPass{.freqHz = 100, .order = 4};

  std::vector<Section> sections;
  REQUIRE(design(peaking, SAMPLE_RATE, sections));
  REQUIRE(design(highPass, SAMPLE_RATE, sections));

  for (Arithmetic arithmetic : {Arithmetic::Fixed, Arithmetic::Float}) {
    CAPTURE(static_cast<int>(arithmetic));
    for (double freq : {50.0, 100.0, 400.0, 1000.0, 5000.0}) {
      CAPTURE(freq);
      // A fresh pipeline each time, so no history carries over
      const auto pipeline = build(
          PipelineSpec{.nodes = {node(peaking), node(highPass)}}, arithmetic);
      CHECK(gainDbAt(*pipeline, freq) ==
            doctest::Approx(designedGainDb(sections, freq)).epsilon(0.01));
    }
  }
}

TEST_CASE("Gain, mixer and delay") {
  SUBCASE("gain scales one channel and leaves the other") {
    const auto pipeline = build(
        PipelineSpec{.nodes = {node(spec::Gain{.gainDb = -6.0206F}, {0})}});
    Channels channels = ramp(300);
    const Channels original = channels;
    run(*pipeline, channels);
    for (size_t i = 0; i < 300; i++) {
      REQUIRE(channels[0][i].toDouble() ==
              doctest::Approx(original[0][i].toDouble() * 0.5).epsilon(1e-5));
    }
    CHECK(channels[1] == original[1]);
  }

  SUBCASE("a mixer can swap the channels") {
    const auto pipeline =
        build(PipelineSpec{.nodes = {node(spec::Mixer{{
                               {.to = 0, .from = {{.channel = 1}}},
                               {.to = 1, .from = {{.channel = 0}}},
                           }})}});
    Channels channels = ramp(300);
    const Channels original = channels;
    run(*pipeline, channels);
    CHECK(channels[0] == original[1]);
    CHECK(channels[1] == original[0]);
  }

  SUBCASE("a delay holds a channel back by its length") {
    const auto pipeline = build(
        PipelineSpec{.nodes = {node(spec::Delay{spec::Samples{3}}, {1})}});
    Channels channels = ramp(300);
    const Channels original = channels;
    run(*pipeline, channels);
    CHECK(channels[0] == original[0]);
    for (size_t i = 0; i < 300; i++) {
      REQUIRE(channels[1][i] == (i < 3 ? Sample::zero() : original[1][i - 3]));
    }
  }
}

TEST_CASE("A spec that does not suit the stream is refused, naming the node") {
  const PipelineSpec spec{.nodes = {node(spec::Gain{.gainDb = 0}),
                                    node(spec::Gain{.gainDb = 0}, {5})}};
  const auto result =
      compile(spec, StreamShape{.sampleRate = 48000, .channels = 2});
  REQUIRE_FALSE(result);
  CHECK(result.error().describe() ==
        "node 1: channel index is beyond the stream's 2 channels");
}

TEST_CASE("The processor runs interleaved PCM and takes a new spec") {
  using bell::audio::SampleFormat;

  Processor processor;
  REQUIRE(processor.setSpec(
      PipelineSpec{.nodes = {node(spec::Gain{.gainDb = -6.0206F})}}));
  REQUIRE(processor.configure(StreamShape{
      .sampleRate = 48000,
      .channels = 2,
      .inputFormat = SampleFormat::S16,
      .outputFormat = SampleFormat::S16,
  }));

  // Enough frames for a fade out and back in around a change
  std::vector<int16_t> buffer(4 * BLOCK_FRAMES * 2);
  const auto process = [&] {
    std::fill(buffer.begin(), buffer.end(), int16_t{20000});
    const auto bytes = std::as_writable_bytes(std::span<int16_t>(buffer));
    REQUIRE(processor.process(bytes, bytes));
    return buffer.back();
  };

  CHECK(process() == 10000);

  // A bad spec is refused and the running one carries on
  CHECK_FALSE(processor.setSpec(
      PipelineSpec{.nodes = {node(spec::Gain{.gainDb = 0}, {5})}}));
  CHECK(process() == 10000);

  REQUIRE(processor.setSpec(PipelineSpec{}));
  CHECK(process() == 20000);
}

#ifndef BELL_DISABLE_TAOJSON
TEST_CASE("JSON is read into a spec, or refused naming the node") {
  const auto parsed = parseTaoJson(tao::json::from_string(R"({
    "version": 1,
    "nodes": [
      { "type": "gain", "channels": [0], "gain_db": -4 },
      { "type": "high_pass", "freq_hz": 80, "order": 4, "alignment": "linkwitz_riley" }
    ]
  })"));
  REQUIRE_MESSAGE(parsed, parsed.error().describe());
  const PipelineSpec expected{
      .nodes = {node(spec::Gain{.gainDb = -4}, {0}),
                node(spec::HighPass{.freqHz = 80,
                                    .order = 4,
                                    .shape = spec::Alignment::LinkwitzRiley})}};
  CHECK(*parsed == expected);

  const auto refused = parseTaoJson(tao::json::from_string(
      R"({"version": 1, "nodes": [{ "type": "gain" }]})"));
  REQUIRE_FALSE(refused);
  CHECK(refused.error().describe() == "node 0: \"gain_db\" is missing");
}
#endif
