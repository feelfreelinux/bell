#include "bell/dsp/Compile.h"

// Standard includes
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>
#include <optional>
#include <utility>
#include <variant>

// Bell includes
#include "bell/dsp/nodes/Biquad.h"
#include "bell/dsp/nodes/Delay.h"
#include "bell/dsp/nodes/Gain.h"
#include "bell/dsp/nodes/Mixer.h"

using namespace bell::dsp;

namespace {
constexpr double PI = 3.14159265358979323846;

constexpr float MIN_GAIN_DB = -120.0F;
constexpr float MAX_GAIN_DB = 40.0F;
constexpr float MAX_MIXER_GAIN_DB = 24.0F;
constexpr float MAX_DELAY_MS = 1000.0F;

double toDb(double linear) {
  return linear > 1.0e-10 ? 20.0 * std::log10(linear) : -200.0;
}

double fromDb(float decibels) {
  return std::pow(10.0, static_cast<double>(decibels) / 20.0);
}

/**
 * @brief What one spec node does, once validated and designed
 */
struct DesignedNode {
  size_t specIndex = 0;

  // Linear work, applied to each of `channels`
  ChannelMask channels;
  double gain = 1.0;
  std::vector<Section> sections;
  size_t delayFrames = 0;

  // Or a mixer, which names its own channels
  std::vector<MixRoute> routes;

  bool isMixer() const { return !routes.empty(); }
};

Expected<void> designParams(const spec::Gain& gain, const StreamShape&,
                            DesignedNode& out) {
  if (!std::isfinite(gain.gainDb) || gain.gainDb < MIN_GAIN_DB ||
      gain.gainDb > MAX_GAIN_DB) {
    return fail("gain must be between -120 and +40 dB");
  }
  out.gain = fromDb(gain.gainDb) * (gain.invert ? -1.0 : 1.0);
  return {};
}

Expected<void> designParams(const spec::Delay& delay, const StreamShape& shape,
                            DesignedNode& out) {
  if (const auto* samples = std::get_if<spec::Samples>(&delay.amount)) {
    if (samples->count > shape.sampleRate) {
      return fail("delay must be at most one second, " +
                  std::to_string(shape.sampleRate) + " samples");
    }
    out.delayFrames = samples->count;
    return {};
  }
  const float delayMs = std::get<spec::Milliseconds>(delay.amount).value;
  if (!std::isfinite(delayMs) || delayMs < 0.0F || delayMs > MAX_DELAY_MS) {
    return fail("delay must be between 0 and 1000 ms");
  }
  out.delayFrames = static_cast<size_t>(
      std::lround(static_cast<double>(delayMs) *
                  static_cast<double>(shape.sampleRate) / 1000.0));
  return {};
}

Expected<void> designParams(const spec::Mixer& mixer, const StreamShape& shape,
                            DesignedNode& out) {
  const std::string limit =
      ", beyond the stream's " + std::to_string(shape.channels) + " channels";

  ChannelMask written;
  for (const spec::MixerRoute& route : mixer.routes) {
    if (route.to >= shape.channels) {
      return fail("a route writes channel " + std::to_string(route.to) + limit);
    }
    if (written.contains(route.to)) {
      return fail("channel " + std::to_string(route.to) +
                  " has more than one route");
    }
    written.add(route.to);
    if (route.from.size() > MAX_CHANNELS) {
      return fail("a route takes at most 8 inputs");
    }

    MixRoute designed{route.to, {}};
    for (const spec::MixerInput& input : route.from) {
      if (input.channel >= shape.channels) {
        return fail("a route reads channel " + std::to_string(input.channel) +
                    limit);
      }
      if (!std::isfinite(input.gainDb) || input.gainDb < MIN_GAIN_DB ||
          input.gainDb > MAX_MIXER_GAIN_DB) {
        return fail("a route input's gain must be between -120 and +24 dB");
      }
      designed.taps.push_back(
          {input.channel, fromDb(input.gainDb) * (input.invert ? -1.0 : 1.0)});
    }

    // A channel copied onto itself is not worth a route
    const bool unchanged = designed.taps.size() == 1 &&
                           designed.taps[0].from == designed.to &&
                           designed.taps[0].factor == 1.0;
    if (!unchanged) {
      out.routes.push_back(std::move(designed));
    }
  }
  return {};
}

// Every other node type is a filter
template <typename Filter>
Expected<void> designParams(const Filter& filter, const StreamShape& shape,
                            DesignedNode& out) {
  return design(filter, static_cast<double>(shape.sampleRate), out.sections);
}

Expected<DesignedNode> designNode(size_t index, const NodeSpec& node,
                                  const StreamShape& shape) {
  DesignedNode designed;
  designed.specIndex = index;

  if (std::holds_alternative<spec::Mixer>(node.params)) {
    if (node.channels) {
      return fail("a mixer names its channels in its routes");
    }
  } else {
    designed.channels =
        node.channels.value_or(ChannelMask::first(shape.channels));
    if (designed.channels.empty()) {
      return fail("no channels selected");
    }
    if (!designed.channels.within(shape.channels)) {
      return fail("channel index is beyond the stream's " +
                  std::to_string(shape.channels) + " channels");
    }
  }

  auto result = std::visit(
      [&](const auto& params) { return designParams(params, shape, designed); },
      node.params);
  if (!result) {
    return nonstd::make_unexpected(result.error());
  }

  std::erase_if(designed.sections,
                [](const Section& section) { return section.isIdentity(); });
  return designed;
}

// Frequency grid for the peak gain estimate
// Log spaced from 10 Hz up, plus every pole and zero where narrow peaks sit
constexpr size_t GRID_POINTS = 128;
constexpr double GRID_LOWEST_HZ = 10.0;

// One frequency w of the grid, as what a section's magnitude needs from it
struct GridPoint {
  double cosW;
  double cos2W;
};

double magnitudeSquared(const Section& s, const GridPoint& point) {
  const double numerator = s.b0 * s.b0 + s.b1 * s.b1 + s.b2 * s.b2 +
                           2.0 * (s.b0 * s.b1 + s.b1 * s.b2) * point.cosW +
                           2.0 * s.b0 * s.b2 * point.cos2W;
  const double denominator = 1.0 + s.a1 * s.a1 + s.a2 * s.a2 +
                             2.0 * (s.a1 + s.a1 * s.a2) * point.cosW +
                             2.0 * s.a2 * point.cos2W;
  return denominator > 0.0 ? std::max(numerator, 0.0) / denominator : 0.0;
}

// Angle of a complex root pair of c0 + c1 z^-1 + c2 z^-2, if it has one
void addRootAngle(double c0, double c1, double c2, std::vector<double>& out) {
  const double product = c0 * c2;
  if (product <= 0.0 || c1 * c1 >= 4.0 * product) {
    return;
  }
  const double cosine = -c1 / (2.0 * std::sqrt(product));
  out.push_back(std::acos(std::clamp(cosine, -1.0, 1.0)));
}

std::vector<GridPoint> makeGrid(const std::vector<DesignedNode>& nodes,
                                double sampleRate) {
  std::vector<double> omegas = {0.0, PI};

  const double lowest = 2.0 * PI * GRID_LOWEST_HZ / sampleRate;
  const double ratio = std::pow(PI / lowest, 1.0 / (GRID_POINTS - 1));
  double omega = lowest;
  for (size_t i = 0; i < GRID_POINTS; i++) {
    omegas.push_back(std::min(omega, PI));
    omega *= ratio;
  }

  for (const DesignedNode& node : nodes) {
    for (const Section& section : node.sections) {
      addRootAngle(1.0, section.a1, section.a2, omegas);
      addRootAngle(section.b0, section.b1, section.b2, omegas);
    }
  }

  std::vector<GridPoint> grid;
  grid.reserve(omegas.size());
  for (double w : omegas) {
    grid.push_back({std::cos(w), std::cos(2.0 * w)});
  }
  return grid;
}

/**
 * @brief The largest gain the nodes apply to a sine on any channel, linear
 */
double peakGain(const std::vector<DesignedNode>& nodes,
                const StreamShape& shape) {
  const std::vector<GridPoint> grid =
      makeGrid(nodes, static_cast<double>(shape.sampleRate));

  // Gain of each channel so far, per grid point
  std::array<std::vector<double>, MAX_CHANNELS> gains;
  for (size_t channel = 0; channel < shape.channels; channel++) {
    gains[channel].assign(grid.size(), 1.0);
  }

  for (const DesignedNode& node : nodes) {
    if (node.isMixer()) {
      // Worst case, the inputs of a route peak together
      std::array<std::vector<double>, MAX_CHANNELS> mixed;
      for (const MixRoute& route : node.routes) {
        mixed[route.to].assign(grid.size(), 0.0);
        for (const MixTap& tap : route.taps) {
          for (size_t point = 0; point < grid.size(); point++) {
            mixed[route.to][point] +=
                std::fabs(tap.factor) * gains[tap.from][point];
          }
        }
      }
      for (const MixRoute& route : node.routes) {
        gains[route.to] = std::move(mixed[route.to]);
      }
      continue;
    }

    for (size_t point = 0; point < grid.size(); point++) {
      double power = node.gain * node.gain;
      for (const Section& section : node.sections) {
        power *= magnitudeSquared(section, grid[point]);
      }
      const double magnitude = std::sqrt(power);
      for (size_t channel = 0; channel < shape.channels; channel++) {
        if (node.channels.contains(channel)) {
          gains[channel][point] *= magnitude;
        }
      }
    }
  }

  double peak = 0.0;
  for (size_t channel = 0; channel < shape.channels; channel++) {
    peak = std::max(
        peak, *std::max_element(gains[channel].begin(), gains[channel].end()));
  }
  return peak;
}

// Linear work collected for one channel and not yet emitted
struct Pending {
  double gain = 1.0;
  std::vector<Section> sections;
  size_t delayFrames = 0;
  std::vector<size_t> specNodes;  // where it came from, for the report

  bool empty() const {
    return gain == 1.0 && sections.empty() && delayFrames == 0;
  }

  bool sameWork(const Pending& other) const {
    return gain == other.gain && sections == other.sections &&
           delayFrames == other.delayFrames;
  }
};

// Merges first-order sections two at a time, each pair is one biquad
void pairFirstOrder(std::vector<Section>& sections) {
  std::optional<size_t> unpaired;
  for (size_t i = 0; i < sections.size();) {
    if (!sections[i].isFirstOrder()) {
      i++;
    } else if (!unpaired) {
      unpaired = i++;
    } else {
      sections[*unpaired] = combineFirstOrder(sections[*unpaired], sections[i]);
      sections.erase(sections.begin() + static_cast<std::ptrdiff_t>(i));
      unpaired.reset();
    }
  }
  std::erase_if(sections,
                [](const Section& section) { return section.isIdentity(); });
}

/**
 * @brief Moves a gain onto sections where it can be applied directly
 *
 * @return the gain left over for a gain node, 1.0 if none
 */
Expected<double> foldGain(std::vector<Section>& sections, double gain,
                          Arithmetic arithmetic) {
  const bool fixed = arithmetic == Arithmetic::Fixed;
  if (fixed) {
    for (Section& section : sections) {
      int halvings = 0;
      while (!section.fitsFixedPoint()) {
        if (++halvings > 16) {
          return fail("filter coefficients are too large to realise");
        }
        section = section.scaled(0.5);
        gain *= 2.0;
      }
    }
  }

  if (sections.empty() || gain == 1.0) {
    return gain;
  }
  if (std::fabs(gain) <= 1.0) {
    sections.front() = sections.front().scaled(gain);
    return 1.0;
  }
  for (auto section = sections.rbegin(); section != sections.rend();
       ++section) {
    if (!fixed || section->scaled(gain).fitsFixedPoint()) {
      *section = section->scaled(gain);
      return 1.0;
    }
  }
  return gain;
}

std::string channelList(ChannelMask channels) {
  std::string text;
  for (size_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (channels.contains(channel)) {
      text += (text.empty() ? "" : ",") + std::to_string(channel);
    }
  }
  return text;
}

std::string specNodeList(const std::vector<size_t>& specNodes) {
  if (specNodes.empty()) {
    return "headroom trim";
  }
  std::string text = specNodes.size() == 1 ? "node " : "nodes ";
  for (size_t i = 0; i < specNodes.size(); i++) {
    text += (i == 0 ? "" : ",") + std::to_string(specNodes[i]);
  }
  return text;
}

std::string decibels(double factor) {
  char text[32];
  std::snprintf(text, sizeof(text), "%+.2f dB", toDb(std::fabs(factor)));
  return std::string(text) + (factor < 0.0 ? " inverted" : "");
}

std::string describeMixer(const DesignedNode& mixer) {
  std::string text = "mixer";
  for (const MixRoute& route : mixer.routes) {
    text += " ch " + std::to_string(route.to) + " =";
    if (route.taps.empty()) {
      text += " silence";
    }
    for (size_t i = 0; i < route.taps.size(); i++) {
      text += std::string(i == 0 ? " " : " + ") + "ch " +
              std::to_string(route.taps[i].from) + " (" +
              decibels(route.taps[i].factor) + ")";
    }
    text += ";";
  }
  return text + " from node " + std::to_string(mixer.specIndex);
}

/**
 * @brief Builder is use to collect all the work that is linear first, and only emit when that run is broken
 *
 * Gains, filters and delays on a channel can run in any order, so they are
 * held as Pending and become one cascade once a mixer reads the channel or
 * the spec ends
 */
class Builder {
 public:
  // `info` carries the trim, which every channel starts with
  Builder(const StreamShape& shape, const CompileOptions& options,
          PipelineInfo info)
      : shape(shape),
        options(options),
        internalDelayLeft(options.internalDelayBytes),
        info(std::move(info)) {
    for (size_t channel = 0; channel < shape.channels; channel++) {
      pending[channel].gain = fromDb(this->info.trimDb);
    }
  }

  Expected<void> add(const DesignedNode& node) {
    if (node.isMixer()) {
      return addMixer(node);
    }
    for (size_t channel = 0; channel < shape.channels; channel++) {
      if (!node.channels.contains(channel)) {
        continue;
      }
      Pending& work = pending[channel];
      work.gain *= node.gain;
      work.sections.insert(work.sections.end(), node.sections.begin(),
                           node.sections.end());
      work.delayFrames += node.delayFrames;
      work.specNodes.push_back(node.specIndex);
    }
    return {};
  }

  Expected<std::unique_ptr<Pipeline>> finish() {
    if (auto emitted = emitPending(ChannelMask::first(shape.channels));
        !emitted) {
      return nonstd::make_unexpected(emitted.error());
    }
    return std::make_unique<Pipeline>(shape, std::move(nodes), std::move(info));
  }

 private:
  // Channels a mixer reads have to be up to date first. Work pending on a
  // channel it only overwrites would never be heard, and is dropped
  Expected<void> addMixer(const DesignedNode& mixer) {
    ChannelMask reads;
    ChannelMask writes;
    for (const MixRoute& route : mixer.routes) {
      writes.add(route.to);
      for (const MixTap& tap : route.taps) {
        reads.add(tap.from);
      }
    }

    if (auto emitted = emitPending(reads); !emitted) {
      return emitted;
    }
    for (size_t channel = 0; channel < shape.channels; channel++) {
      if (writes.contains(channel)) {
        pending[channel] = {};
      }
    }
    push(std::make_unique<MixerNode>(mixer.routes, options.memory),
         describeMixer(mixer));
    return {};
  }

  // Emits what is pending on each of `channels`
  Expected<void> emitPending(ChannelMask channels) {
    for (size_t channel = 0; channel < shape.channels; channel++) {
      if (!channels.contains(channel) || pending[channel].empty()) {
        continue;
      }
      Pending work = std::exchange(pending[channel], {});

      // Channels asked to do the same thing share one node
      ChannelMask group{channel};
      for (size_t other = channel + 1; other < shape.channels; other++) {
        if (channels.contains(other) && pending[other].sameWork(work)) {
          group.add(other);
          work.specNodes.insert(work.specNodes.end(),
                                pending[other].specNodes.begin(),
                                pending[other].specNodes.end());
          pending[other] = {};
        }
      }
      std::sort(work.specNodes.begin(), work.specNodes.end());
      work.specNodes.erase(
          std::unique(work.specNodes.begin(), work.specNodes.end()),
          work.specNodes.end());

      if (auto emitted = emit(group, std::move(work)); !emitted) {
        return emitted;
      }
    }
    return {};
  }

  // Returns one channel's work as nodes
  Expected<void> emit(ChannelMask group, Pending work) {
    const std::string origin = " on ch " + channelList(group) + " from " +
                               specNodeList(work.specNodes);

    pairFirstOrder(work.sections);
    const auto gain = foldGain(work.sections, work.gain, options.arithmetic);
    if (!gain) {
      return nonstd::make_unexpected(gain.error());
    }

    if (!work.sections.empty()) {
      info.sections += work.sections.size() * group.count();
      push(std::make_unique<BiquadCascade>(group, work.sections, options.memory,
                                           options.arithmetic),
           "biquad x" + std::to_string(work.sections.size()) + origin);
    }

    if (*gain != 1.0) {
      if (std::fabs(*gain) > GainNode::MAX_FACTOR) {
        return fail("total gain on channel " + channelList(group) +
                    " is more than +42 dB");
      }
      push(std::make_unique<GainNode>(group, *gain),
           "gain " + decibels(*gain) + origin);
    }

    if (work.delayFrames > 0) {
      // Allocate the lines for delay on iram first,
      // So most of the cases are PSRAM-free
      const size_t bytes = group.count() * work.delayFrames * sizeof(Sample);
      const bool internal = bytes <= internalDelayLeft;
      if (internal) {
        internalDelayLeft -= bytes;
      }
      push(std::make_unique<DelayNode>(
               group, work.delayFrames,
               internal ? options.memory : options.bulkMemory),
           "delay " + std::to_string(work.delayFrames) + " frames" + origin +
               (internal ? "" : " (bulk memory)"));
    }
    return {};
  }

  void push(std::unique_ptr<Node> node, std::string description) {
    nodes.push_back(std::move(node));
    info.nodes.push_back(std::move(description));
  }

  StreamShape shape;
  const CompileOptions& options;

  std::array<Pending, MAX_CHANNELS> pending;

  // What is left of CompileOptions::internalDelayBytes
  size_t internalDelayLeft;

  std::vector<std::unique_ptr<Node>> nodes;
  PipelineInfo info;
};
}  // namespace

Expected<std::unique_ptr<Pipeline>> bell::dsp::compile(
    const PipelineSpec& spec, const StreamShape& shape,
    const CompileOptions& options) try {
  if (shape.channels == 0 || shape.channels > MAX_CHANNELS) {
    return fail("a stream has between 1 and 8 channels");
  }
  if (shape.sampleRate == 0) {
    return fail("sample rate must be above zero");
  }

  std::vector<DesignedNode> designed;
  for (size_t index = 0; index < spec.nodes.size(); index++) {
    if (!spec.nodes[index].enabled) {
      continue;
    }
    auto node = designNode(index, spec.nodes[index], shape);
    if (!node) {
      return nonstd::make_unexpected(Error{index, node.error().message});
    }
    designed.push_back(std::move(*node));
  }

  PipelineInfo info;
  info.arithmetic = options.arithmetic;
  info.peakGainDb = static_cast<float>(toDb(peakGain(designed, shape)));
  const float excess = info.peakGainDb + options.headroomMarginDb;
  if (options.headroom == Headroom::Trim && excess > 0.0F) {
    info.trimDb = -excess;
  }

  Builder builder(shape, options, std::move(info));
  for (const DesignedNode& node : designed) {
    if (auto added = builder.add(node); !added) {
      return nonstd::make_unexpected(
          Error{node.specIndex, added.error().message});
    }
  }
  return builder.finish();
} catch (const std::bad_alloc&) {
  // Only a long delay can be large here. What was built so far is already
  // released
  return fail("not enough memory for this pipeline");
}

Arithmetic bell::dsp::defaultArithmetic() {
#ifdef BELL_DSP_FLOAT_FILTERS
  return Arithmetic::Float;
#else
  return Arithmetic::Fixed;
#endif
}

std::string PipelineInfo::describe() const {
  char header[112];
  std::snprintf(header, sizeof(header),
                "peak gain %+.2f dB, trim %+.2f dB, %zu biquad sections (%s)",
                static_cast<double>(peakGainDb), static_cast<double>(trimDb),
                sections, arithmetic == Arithmetic::Float ? "float" : "fixed");
  std::string text = header;
  for (const std::string& node : nodes) {
    text += "\n  " + node;
  }
  return text;
}
