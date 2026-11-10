#ifndef BELL_DISABLE_TAOJSON
#include "bell/dsp/TaoJson.h"

// Standard includes
#include <cmath>
#include <set>
#include <stdexcept>
#include <string_view>

// Library includes
#include <tao/json.hpp>

using namespace bell::dsp;

namespace {
constexpr int SUPPORTED_VERSION = 1;

// Thrown anywhere below and turned into an Error at the node boundary, so
// the per-type readers need no error plumbing
struct ParseFailure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/**
 * @brief Reads the members of one JSON object and remembers which were used,
 *        so anything left over can be reported as unknown
 */
class Members {
 public:
  explicit Members(const tao::json::value& json) {
    if (!json.is_object()) {
      throw ParseFailure("expected an object");
    }
    object = &json.get_object();
  }

  bool has(std::string_view key) const {
    return object->find(key) != object->end();
  }

  double number(std::string_view key) {
    const tao::json::value& value = required(key);
    if (!value.is_number()) {
      throw ParseFailure("\"" + std::string(key) + "\" must be a number");
    }
    return value.as<double>();
  }

  double number(std::string_view key, double fallback) {
    return has(key) ? number(key) : fallback;
  }

  int integer(std::string_view key, int fallback) {
    return has(key) ? integer(key) : fallback;
  }

  int integer(std::string_view key) {
    const double value = number(key);
    if (value != std::floor(value) || value < 0.0 || value > 255.0) {
      throw ParseFailure("\"" + std::string(key) +
                         "\" must be a small whole number");
    }
    return static_cast<int>(value);
  }

  bool boolean(std::string_view key, bool fallback) {
    if (!has(key)) {
      return fallback;
    }
    const tao::json::value& value = required(key);
    if (!value.is_boolean()) {
      throw ParseFailure("\"" + std::string(key) + "\" must be true or false");
    }
    return value.get_boolean();
  }

  std::string string(std::string_view key) {
    const tao::json::value& value = required(key);
    if (!value.is_string()) {
      throw ParseFailure("\"" + std::string(key) + "\" must be a string");
    }
    return value.get_string();
  }

  std::string string(std::string_view key, const std::string& fallback) {
    return has(key) ? string(key) : fallback;
  }

  const tao::json::value& required(std::string_view key) {
    const auto found = object->find(key);
    if (found == object->end()) {
      throw ParseFailure("\"" + std::string(key) + "\" is missing");
    }
    used.insert(found->first);
    return found->second;
  }

  // Call once everything expected has been read
  void rejectUnknown() const {
    for (const auto& [key, value] : *object) {
      if (used.find(key) == used.end()) {
        throw ParseFailure("unknown key \"" + key + "\"");
      }
    }
  }

 private:
  const tao::json::value::object_t* object;
  std::set<std::string_view> used;
};

void rejectBoth(const Members& members, std::string_view first,
                std::string_view second) {
  if (members.has(first) && members.has(second)) {
    throw ParseFailure("give either \"" + std::string(first) + "\" or \"" +
                       std::string(second) + "\", not both");
  }
}

// A width that must be stated one way or the other
std::variant<spec::Q, spec::Bandwidth> requiredWidth(Members& members) {
  rejectBoth(members, "q", "bandwidth_oct");
  if (members.has("bandwidth_oct")) {
    return spec::Bandwidth{static_cast<float>(members.number("bandwidth_oct"))};
  }
  return spec::Q{static_cast<float>(members.number("q"))};
}

// order, with the keys that only make sense for order 2 refused otherwise
uint8_t orderOf(Members& members, std::string_view first,
                std::string_view second) {
  const int order = members.integer("order", 2);
  if (order != 2) {
    for (std::string_view key : {first, second}) {
      if (members.has(key)) {
        throw ParseFailure("\"" + std::string(key) +
                           "\" only applies to order 2");
      }
    }
  }
  return static_cast<uint8_t>(order);
}

template <typename Shelf>
Shelf readShelf(Members& members) {
  Shelf shelf;
  shelf.freqHz = static_cast<float>(members.number("freq_hz"));
  shelf.gainDb = static_cast<float>(members.number("gain_db"));
  shelf.order = orderOf(members, "q", "slope_db_oct");
  rejectBoth(members, "q", "slope_db_oct");
  if (members.has("slope_db_oct")) {
    shelf.width =
        spec::Slope{static_cast<float>(members.number("slope_db_oct"))};
  } else if (members.has("q")) {
    shelf.width = spec::Q{static_cast<float>(members.number("q"))};
  }
  return shelf;
}

template <typename Pass>
Pass readPass(Members& members) {
  Pass pass;
  pass.freqHz = static_cast<float>(members.number("freq_hz"));
  pass.order = static_cast<uint8_t>(members.integer("order", 2));
  rejectBoth(members, "q", "alignment");
  if (members.has("q")) {
    pass.shape = spec::Q{static_cast<float>(members.number("q"))};
  } else if (members.has("alignment")) {
    const std::string alignment = members.string("alignment");
    if (alignment == "butterworth") {
      pass.shape = spec::Alignment::Butterworth;
    } else if (alignment == "linkwitz_riley") {
      pass.shape = spec::Alignment::LinkwitzRiley;
    } else {
      throw ParseFailure(
          "\"alignment\" must be \"butterworth\" or \"linkwitz_riley\"");
    }
  }
  return pass;
}

template <typename Band>
Band readBand(Members& members) {
  Band band;
  band.freqHz = static_cast<float>(members.number("freq_hz"));
  band.width = requiredWidth(members);
  return band;
}

uint8_t channelIndex(Members& members, std::string_view key) {
  const int index = members.integer(key);
  if (index >= static_cast<int>(MAX_CHANNELS)) {
    throw ParseFailure("\"" + std::string(key) +
                       "\" must be a channel index from 0 to 7");
  }
  return static_cast<uint8_t>(index);
}

const tao::json::value::array_t& listOf(Members& members,
                                        std::string_view key) {
  const tao::json::value& value = members.required(key);
  if (!value.is_array()) {
    throw ParseFailure("\"" + std::string(key) + "\" must be a list");
  }
  return value.get_array();
}

spec::Mixer readMixer(Members& members) {
  spec::Mixer mixer;
  for (const tao::json::value& routeJson : listOf(members, "routes")) {
    Members routeMembers(routeJson);
    spec::MixerRoute route;
    route.to = channelIndex(routeMembers, "to");
    for (const tao::json::value& inputJson : listOf(routeMembers, "from")) {
      Members inputMembers(inputJson);
      spec::MixerInput input;
      input.channel = channelIndex(inputMembers, "ch");
      input.gainDb = static_cast<float>(inputMembers.number("gain_db", 0.0));
      input.invert = inputMembers.boolean("invert", false);
      inputMembers.rejectUnknown();
      route.from.push_back(input);
    }
    routeMembers.rejectUnknown();
    mixer.routes.push_back(std::move(route));
  }
  return mixer;
}

NodeParams readParams(const std::string& type, Members& members) {
  if (type == "gain") {
    return spec::Gain{static_cast<float>(members.number("gain_db")),
                      members.boolean("invert", false)};
  }
  if (type == "peaking") {
    spec::Peaking peaking;
    peaking.freqHz = static_cast<float>(members.number("freq_hz"));
    peaking.gainDb = static_cast<float>(members.number("gain_db"));
    peaking.width = requiredWidth(members);
    return peaking;
  }
  if (type == "low_shelf") {
    return readShelf<spec::LowShelf>(members);
  }
  if (type == "high_shelf") {
    return readShelf<spec::HighShelf>(members);
  }
  if (type == "low_pass") {
    return readPass<spec::LowPass>(members);
  }
  if (type == "high_pass") {
    return readPass<spec::HighPass>(members);
  }
  if (type == "band_pass") {
    return readBand<spec::BandPass>(members);
  }
  if (type == "notch") {
    return readBand<spec::Notch>(members);
  }
  if (type == "all_pass") {
    spec::AllPass allPass;
    allPass.freqHz = static_cast<float>(members.number("freq_hz"));
    allPass.order = orderOf(members, "q", "bandwidth_oct");
    if (allPass.order == 2) {
      allPass.width = requiredWidth(members);
    }
    return allPass;
  }
  if (type == "biquad") {
    return spec::Biquad{members.number("b0"), members.number("b1"),
                        members.number("b2"), members.number("a1"),
                        members.number("a2")};
  }
  if (type == "mixer") {
    return readMixer(members);
  }
  if (type == "delay") {
    rejectBoth(members, "delay_ms", "delay_samples");
    if (members.has("delay_samples")) {
      const double count = members.number("delay_samples");
      if (count != std::floor(count) || count < 0.0 || count > 1.0e6) {
        throw ParseFailure("\"delay_samples\" must be a whole number");
      }
      return spec::Delay{spec::Samples{static_cast<uint32_t>(count)}};
    }
    return spec::Delay{
        spec::Milliseconds{static_cast<float>(members.number("delay_ms"))}};
  }
  throw ParseFailure("unknown type \"" + type + "\"");
}

std::optional<ChannelMask> readChannels(Members& members) {
  if (!members.has("channels")) {
    return std::nullopt;
  }
  const tao::json::value& list = members.required("channels");
  if (!list.is_array()) {
    throw ParseFailure("\"channels\" must be a list of channel indices");
  }
  ChannelMask mask;
  for (const tao::json::value& entry : list.get_array()) {
    const double index = entry.is_number() ? entry.as<double>() : -1.0;
    if (index != std::floor(index) || index < 0.0 ||
        index >= static_cast<double>(MAX_CHANNELS)) {
      throw ParseFailure(
          "\"channels\" entries must be whole numbers from 0 to 7");
    }
    mask.add(static_cast<size_t>(index));
  }
  return mask;
}

NodeSpec readNode(const tao::json::value& json) {
  Members members(json);
  NodeSpec node;
  const std::string type = members.string("type");
  node.channels = readChannels(members);
  node.enabled = members.boolean("enabled", true);
  node.id = members.string("id", "");
  node.params = readParams(type, members);
  if (node.channels && std::holds_alternative<spec::Mixer>(node.params)) {
    throw ParseFailure(
        "\"channels\" does not apply to a mixer; its routes name them");
  }
  members.rejectUnknown();
  return node;
}
}  // namespace

Expected<PipelineSpec> bell::dsp::parseTaoJson(const tao::json::value& json) {
  PipelineSpec spec;
  std::optional<size_t> nodeIndex;
  try {
    Members root(json);
    if (root.integer("version", -1) != SUPPORTED_VERSION) {
      throw ParseFailure("\"version\" must be 1");
    }
    const tao::json::value& nodes = root.required("nodes");
    if (!nodes.is_array()) {
      throw ParseFailure("\"nodes\" must be a list");
    }
    root.rejectUnknown();

    for (size_t index = 0; index < nodes.get_array().size(); index++) {
      nodeIndex = index;
      spec.nodes.push_back(readNode(nodes.get_array()[index]));
    }
  } catch (const std::exception& failure) {
    return nonstd::make_unexpected(Error{nodeIndex, failure.what()});
  }
  return spec;
}

#endif  // BELL_DISABLE_TAOJSON
