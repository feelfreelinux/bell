#include "bell/dsp/nodes/Biquad.h"

// Standard includes
#include <algorithm>
#include <cmath>

using namespace bell::dsp;

namespace {
constexpr double PI = 3.14159265358979323846;

// Parameter ranges, wide enough for any real tuning. They keep a typo or a
// unit mix-up from becoming an unstable filter
constexpr float MAX_FILTER_GAIN_DB = 40.0F;
constexpr float MAX_Q = 100.0F;
constexpr float MAX_BANDWIDTH_OCTAVES = 10.0F;
constexpr int MAX_ORDER = 8;

// Limits of the fixed point kernel, see Section::fitsFixedPoint()
constexpr double MAX_COEFFICIENT = 7.999;
constexpr double MAX_COEFFICIENT_SUM = 15.99;

// The quantities every design starts from
struct Angle {
  double omega;  // radians per sample
  double cosine;
  double sine;
};

Expected<Angle> angleOf(float freqHz, double sampleRate) {
  if (!std::isfinite(freqHz) || freqHz <= 0.0F) {
    return fail("frequency must be above 0 Hz");
  }
  if (static_cast<double>(freqHz) >= sampleRate / 2.0) {
    return fail("frequency must be below half the sample rate");
  }
  const double omega = 2.0 * PI * static_cast<double>(freqHz) / sampleRate;
  return Angle{omega, std::cos(omega), std::sin(omega)};
}

Expected<double> amplitudeOf(float gainDb) {
  if (!std::isfinite(gainDb) || std::fabs(gainDb) > MAX_FILTER_GAIN_DB) {
    return fail("gain must be within +-40 dB");
  }
  // Square root of the linear gain, the cookbook formulas split a boost
  // between numerator and denominator
  return std::pow(10.0, static_cast<double>(gainDb) / 40.0);
}

Expected<double> alphaOf(const spec::Q& q, const Angle& angle) {
  if (!std::isfinite(q.value) || q.value <= 0.0F || q.value > MAX_Q) {
    return fail("q must be above 0 and at most 100");
  }
  return angle.sine / (2.0 * static_cast<double>(q.value));
}

Expected<double> alphaOf(const spec::Bandwidth& bandwidth, const Angle& angle) {
  if (!std::isfinite(bandwidth.octaves) || bandwidth.octaves <= 0.0F ||
      bandwidth.octaves > MAX_BANDWIDTH_OCTAVES) {
    return fail("bandwidth must be above 0 and at most 10 octaves");
  }
  return angle.sine * std::sinh(std::log(2.0) / 2.0 *
                                static_cast<double>(bandwidth.octaves) *
                                angle.omega / angle.sine);
}

Expected<double> alphaOf(const std::variant<spec::Q, spec::Bandwidth>& width,
                         const Angle& angle) {
  return std::visit([&](const auto& form) { return alphaOf(form, angle); },
                    width);
}

// Shelf "alpha" from either a Q or a slope, given the amplitude A
Expected<double> shelfAlphaOf(const std::variant<spec::Q, spec::Slope>& width,
                              const Angle& angle, double amplitude) {
  if (const auto* q = std::get_if<spec::Q>(&width)) {
    return alphaOf(*q, angle);
  }
  const float slope = std::get<spec::Slope>(width).dbPerOctave;
  if (!std::isfinite(slope) || slope <= 0.0F || slope > 12.0F) {
    return fail("slope must be above 0 and at most 12 dB per octave");
  }
  const double steepness = static_cast<double>(slope) / 12.0;
  return angle.sine / 2.0 *
         std::sqrt((amplitude + 1.0 / amplitude) * (1.0 / steepness - 1.0) +
                   2.0);
}

Section normalised(double b0, double b1, double b2, double a0, double a1,
                   double a2) {
  return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

// Second-order low and high pass with an explicit Q
Section secondOrderPass(bool highPass, const Angle& angle, double q) {
  const double alpha = angle.sine / (2.0 * q);
  if (highPass) {
    const double onePlusCos = 1.0 + angle.cosine;
    return normalised(onePlusCos / 2.0, -onePlusCos, onePlusCos / 2.0,
                      1.0 + alpha, -2.0 * angle.cosine, 1.0 - alpha);
  }
  // 1 - cos(w) as 2 sin^2(w/2), the direct form loses digits at low
  // frequencies where cos(w) is within rounding of 1
  const double halfSine = std::sin(angle.omega / 2.0);
  const double oneMinusCos = 2.0 * halfSine * halfSine;
  return normalised(oneMinusCos / 2.0, oneMinusCos, oneMinusCos / 2.0,
                    1.0 + alpha, -2.0 * angle.cosine, 1.0 - alpha);
}

Section firstOrderPass(bool highPass, const Angle& angle) {
  const double k = std::tan(angle.omega / 2.0);
  const double a1 = -(1.0 - k) / (1.0 + k);
  if (highPass) {
    return {1.0 / (1.0 + k), -1.0 / (1.0 + k), 0.0, a1, 0.0};
  }
  return {k / (1.0 + k), k / (1.0 + k), 0.0, a1, 0.0};
}

// Butterworth of any order, order/2 second-order sections, plus a first-order one when the order is odd
void butterworth(bool highPass, const Angle& angle, int order,
                 std::vector<Section>& out) {
  if (order % 2 == 1) {
    out.push_back(firstOrderPass(highPass, angle));
  }
  for (int k = order / 2 - 1; k >= 0; k--) {
    const double q =
        1.0 / (2.0 * std::sin(PI * (2.0 * k + 1.0) / (2.0 * order)));
    out.push_back(secondOrderPass(highPass, angle, q));
  }
}

Expected<void> designPass(bool highPass, float freqHz, int order,
                          const std::variant<spec::Alignment, spec::Q>& shape,
                          double sampleRate, std::vector<Section>& out) {
  const auto angle = angleOf(freqHz, sampleRate);
  if (!angle) {
    return nonstd::make_unexpected(angle.error());
  }
  if (order < 1 || order > MAX_ORDER) {
    return fail("order must be between 1 and 8");
  }

  if (const auto* q = std::get_if<spec::Q>(&shape)) {
    if (order != 2) {
      return fail("q only applies to order 2; use an alignment instead");
    }
    if (!std::isfinite(q->value) || q->value <= 0.0F || q->value > MAX_Q) {
      return fail("q must be above 0 and at most 100");
    }
    out.push_back(
        secondOrderPass(highPass, *angle, static_cast<double>(q->value)));
    return {};
  }

  switch (std::get<spec::Alignment>(shape)) {
    case spec::Alignment::Butterworth:
      butterworth(highPass, *angle, order, out);
      return {};
    case spec::Alignment::LinkwitzRiley:
      if (order % 2 != 0) {
        return fail("a Linkwitz-Riley filter needs an even order");
      }
      // Two Butterworth filters of half the order in series
      butterworth(highPass, *angle, order / 2, out);
      butterworth(highPass, *angle, order / 2, out);
      return {};
  }
  return fail("unknown alignment");
}

Expected<void> designShelf(bool highShelf, float freqHz, float gainDb,
                           int order,
                           const std::variant<spec::Q, spec::Slope>& width,
                           double sampleRate, std::vector<Section>& out) {
  const auto angle = angleOf(freqHz, sampleRate);
  if (!angle) {
    return nonstd::make_unexpected(angle.error());
  }
  const auto amplitude = amplitudeOf(gainDb);
  if (!amplitude) {
    return nonstd::make_unexpected(amplitude.error());
  }
  const double A = *amplitude;

  if (order == 1) {
    const double t = std::tan(angle->omega / 2.0);
    if (highShelf) {
      out.push_back(normalised(A * t + A * A, A * t - A * A, 0.0, A * t + 1.0,
                               A * t - 1.0, 0.0));
    } else {
      out.push_back(
          normalised(A * A * t + A, A * A * t - A, 0.0, t + A, t - A, 0.0));
    }
    return {};
  }
  if (order != 2) {
    return fail("a shelf is order 1 or 2");
  }

  const auto alpha = shelfAlphaOf(width, *angle, A);
  if (!alpha) {
    return nonstd::make_unexpected(alpha.error());
  }
  const double c = angle->cosine;
  const double beta = 2.0 * std::sqrt(A) * *alpha;

  if (highShelf) {
    out.push_back(normalised(A * ((A + 1.0) + (A - 1.0) * c + beta),
                             -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
                             A * ((A + 1.0) + (A - 1.0) * c - beta),
                             (A + 1.0) - (A - 1.0) * c + beta,
                             2.0 * ((A - 1.0) - (A + 1.0) * c),
                             (A + 1.0) - (A - 1.0) * c - beta));
  } else {
    out.push_back(normalised(A * ((A + 1.0) - (A - 1.0) * c + beta),
                             2.0 * A * ((A - 1.0) - (A + 1.0) * c),
                             A * ((A + 1.0) - (A - 1.0) * c - beta),
                             (A + 1.0) + (A - 1.0) * c + beta,
                             -2.0 * ((A - 1.0) + (A + 1.0) * c),
                             (A + 1.0) + (A - 1.0) * c - beta));
  }
  return {};
}
}  // namespace

bool Section::isIdentity() const {
  constexpr double TOLERANCE = 1.0e-12;
  return std::fabs(b0 - 1.0) < TOLERANCE && std::fabs(b1 - a1) < TOLERANCE &&
         std::fabs(b2 - a2) < TOLERANCE;
}

std::complex<double> Section::response(double omega) const {
  const std::complex<double> z1 = std::polar(1.0, -omega);
  const std::complex<double> z2 = z1 * z1;
  return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
}

bool Section::fitsFixedPoint() const {
  double sum = 0.0;
  for (double coefficient : {b0, b1, b2, a1, a2}) {
    if (!std::isfinite(coefficient) ||
        std::fabs(coefficient) > MAX_COEFFICIENT) {
      return false;
    }
    sum += std::fabs(coefficient);
  }
  return sum <= MAX_COEFFICIENT_SUM;
}

Section bell::dsp::combineFirstOrder(const Section& first,
                                     const Section& second) {
  // (b0 + b1 z)(b0' + b1' z), and the same for the denominators
  return {first.b0 * second.b0, first.b0 * second.b1 + first.b1 * second.b0,
          first.b1 * second.b1, first.a1 + second.a1, first.a1 * second.a1};
}

Expected<void> bell::dsp::design(const spec::Peaking& filter, double sampleRate,
                                 std::vector<Section>& out) {
  const auto angle = angleOf(filter.freqHz, sampleRate);
  if (!angle) {
    return nonstd::make_unexpected(angle.error());
  }
  const auto amplitude = amplitudeOf(filter.gainDb);
  if (!amplitude) {
    return nonstd::make_unexpected(amplitude.error());
  }
  const auto alpha = alphaOf(filter.width, *angle);
  if (!alpha) {
    return nonstd::make_unexpected(alpha.error());
  }
  const double A = *amplitude;
  out.push_back(normalised(1.0 + *alpha * A, -2.0 * angle->cosine,
                           1.0 - *alpha * A, 1.0 + *alpha / A,
                           -2.0 * angle->cosine, 1.0 - *alpha / A));
  return {};
}

Expected<void> bell::dsp::design(const spec::LowShelf& filter,
                                 double sampleRate, std::vector<Section>& out) {
  return designShelf(false, filter.freqHz, filter.gainDb, filter.order,
                     filter.width, sampleRate, out);
}

Expected<void> bell::dsp::design(const spec::HighShelf& filter,
                                 double sampleRate, std::vector<Section>& out) {
  return designShelf(true, filter.freqHz, filter.gainDb, filter.order,
                     filter.width, sampleRate, out);
}

Expected<void> bell::dsp::design(const spec::LowPass& filter, double sampleRate,
                                 std::vector<Section>& out) {
  return designPass(false, filter.freqHz, filter.order, filter.shape,
                    sampleRate, out);
}

Expected<void> bell::dsp::design(const spec::HighPass& filter,
                                 double sampleRate, std::vector<Section>& out) {
  return designPass(true, filter.freqHz, filter.order, filter.shape, sampleRate,
                    out);
}

Expected<void> bell::dsp::design(const spec::BandPass& filter,
                                 double sampleRate, std::vector<Section>& out) {
  const auto angle = angleOf(filter.freqHz, sampleRate);
  if (!angle) {
    return nonstd::make_unexpected(angle.error());
  }
  const auto alpha = alphaOf(filter.width, *angle);
  if (!alpha) {
    return nonstd::make_unexpected(alpha.error());
  }
  out.push_back(normalised(*alpha, 0.0, -*alpha, 1.0 + *alpha,
                           -2.0 * angle->cosine, 1.0 - *alpha));
  return {};
}

Expected<void> bell::dsp::design(const spec::Notch& filter, double sampleRate,
                                 std::vector<Section>& out) {
  const auto angle = angleOf(filter.freqHz, sampleRate);
  if (!angle) {
    return nonstd::make_unexpected(angle.error());
  }
  const auto alpha = alphaOf(filter.width, *angle);
  if (!alpha) {
    return nonstd::make_unexpected(alpha.error());
  }
  out.push_back(normalised(1.0, -2.0 * angle->cosine, 1.0, 1.0 + *alpha,
                           -2.0 * angle->cosine, 1.0 - *alpha));
  return {};
}

Expected<void> bell::dsp::design(const spec::AllPass& filter, double sampleRate,
                                 std::vector<Section>& out) {
  const auto angle = angleOf(filter.freqHz, sampleRate);
  if (!angle) {
    return nonstd::make_unexpected(angle.error());
  }
  if (filter.order == 1) {
    const double t = std::tan(angle->omega / 2.0);
    const double c = (t - 1.0) / (t + 1.0);
    out.push_back({c, 1.0, 0.0, c, 0.0});
    return {};
  }
  if (filter.order != 2) {
    return fail("an all pass is order 1 or 2");
  }
  const auto alpha = alphaOf(filter.width, *angle);
  if (!alpha) {
    return nonstd::make_unexpected(alpha.error());
  }
  out.push_back(normalised(1.0 - *alpha, -2.0 * angle->cosine, 1.0 + *alpha,
                           1.0 + *alpha, -2.0 * angle->cosine, 1.0 - *alpha));
  return {};
}

Expected<void> bell::dsp::design(const spec::Biquad& filter,
                                 double /*sampleRate*/,
                                 std::vector<Section>& out) {
  for (double coefficient :
       {filter.b0, filter.b1, filter.b2, filter.a1, filter.a2}) {
    if (!std::isfinite(coefficient)) {
      return fail("coefficients must be finite numbers");
    }
  }
  // Both poles inside the unit circle
  if (std::fabs(filter.a2) >= 1.0 || std::fabs(filter.a1) >= 1.0 + filter.a2) {
    return fail("coefficients describe an unstable filter");
  }
  out.push_back({filter.b0, filter.b1, filter.b2, filter.a1, filter.a2});
  return {};
}

BiquadCascade::BiquadCascade(ChannelMask channels,
                             std::span<const Section> sections,
                             std::pmr::memory_resource* memory,
                             Arithmetic arithmetic)
    : channels(channels),
      arithmetic(arithmetic),
      stages(memory),
      histories(memory),
      floatStages(memory),
      floatHistories(memory),
      scratch(memory) {
  using fixed::Coeff;

  if (arithmetic == Arithmetic::Float) {
    floatStages.reserve(sections.size());
    for (const Section& section : sections) {
      floatStages.push_back(toFloatStage(section));
    }
    floatHistories.assign(channels.count() * floatStages.size(),
                          FloatHistory{});
    scratch.assign(BLOCK_FRAMES, 0.0F);
    return;
  }

  stages.reserve(sections.size());
  for (const Section& section : sections) {
    stages.push_back({
        .b0 = Coeff::fromDouble(section.b0),
        .b1 = Coeff::fromDouble(section.b1),
        .b2 = Coeff::fromDouble(section.b2),
        .a1 = Coeff::fromDouble(-section.a1),
        .a2 = Coeff::fromDouble(-section.a2),
    });
  }
  histories.assign(channels.count() * stages.size(), History{});
}

void BiquadCascade::reset() noexcept {
  for (History& history : histories) {
    history = History{};
  }
  for (FloatHistory& history : floatHistories) {
    history = FloatHistory{};
  }
}

void BiquadCascade::process(Block& block, const Context& /*context*/) noexcept {
  if (arithmetic == Arithmetic::Float) {
    processFloat(block);
  } else {
    processFixed(block);
  }
}

void BiquadCascade::processFixed(Block& block) noexcept {
  // One stage over the whole block before the next, a single stage's
  // coefficients and history are about all that fits in registers
  size_t slot = 0;
  for (size_t channel = 0; channel < block.numChannels; channel++) {
    if (!channels.contains(channel)) {
      continue;
    }
    const std::span<Sample> samples = block.channel(channel);
    History* history = &histories[slot * stages.size()];
    for (const Coefficients& stage : stages) {
      runSecondOrder(samples, stage, *history);
      history++;
    }
    slot++;
  }
}

// Float kernel
namespace {
// The largest float below 2^31 and -2^31 itself, the range of a raw sample
constexpr float RAW_CEILING = 2147483520.0F;
constexpr float RAW_FLOOR = -2147483648.0F;

// A filter state outside this range is cleared: below it the state would
// wander into denormals, above it (or NaN) it came from a fault and would
// stay forever. In raw sample units
constexpr float STATE_FLOOR = 1.0e-9F;
constexpr float STATE_CEILING = 1.0e30F;

inline float settled(float state) {
  const float magnitude = std::fabs(state);
  return magnitude >= STATE_FLOOR && magnitude <= STATE_CEILING ? state : 0.0F;
}

inline Sample toSample(float value) {
  if (value >= RAW_CEILING) {
    return Sample::fromRaw(Sample::RAW_MAX);
  }
  if (value > RAW_FLOOR) {
    return Sample::fromRaw(static_cast<int32_t>(value));
  }
  return Sample::fromRaw(Sample::RAW_MIN);
}
}  // namespace

BiquadCascade::FloatStage BiquadCascade::toFloatStage(const Section& section) {
  // Undo the bilinear transform to get the analogue prototype of the section,
  //
  //     m0 s^2 + (m0 k + m1) g s + (m0 + m2) g^2
  //     ----------------------------------------
  //              s^2 + k g s + g^2
  //
  // which a state-variable filter with integrator gain g and damping k gives
  // when its outputs are mixed as m0 * input + m1 * band + m2 * low
  const double scale = 1.0 - section.a1 + section.a2;
  const double gSquared = (1.0 + section.a1 + section.a2) / scale;
  const double g = std::sqrt(gSquared);
  const double k = 2.0 * (1.0 - section.a2) / (scale * g);

  const double m0 = (section.b0 - section.b1 + section.b2) / scale;
  const double m1 = 2.0 * (section.b0 - section.b2) / (scale * g) - m0 * k;
  const double m2 =
      (section.b0 + section.b1 + section.b2) / (scale * gSquared) - m0;

  const double a1 = 1.0 / (1.0 + g * (g + k));
  const double a2 = g * a1;
  const double a3 = g * a2;

  for (double value : {a1, a2, a3, m0, m1, m2}) {
    if (!std::isfinite(value)) {
      // Not a stable section, pass the signal through
      return {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F};
    }
  }
  return {static_cast<float>(a1), static_cast<float>(a2),
          static_cast<float>(a3), static_cast<float>(m0),
          static_cast<float>(m1), static_cast<float>(m2)};
}

void BiquadCascade::processFloat(Block& block) noexcept {
  const size_t frames = std::min(block.frames, scratch.size());
  float* const work = scratch.data();

  size_t slot = 0;
  for (size_t channel = 0; channel < block.numChannels; channel++) {
    if (!channels.contains(channel)) {
      continue;
    }
    Sample* const samples = block.data[channel];

    // Floats carry the raw sample values, a filter does not care about scale
    for (size_t i = 0; i < frames; i++) {
      work[i] = static_cast<float>(samples[i].raw);
    }

    FloatHistory* history = &floatHistories[slot * floatStages.size()];
    for (const FloatStage& stage : floatStages) {
      runFloat(work, frames, stage, *history);
      history++;
    }

    for (size_t i = 0; i < frames; i++) {
      samples[i] = toSample(work[i]);
    }
    slot++;
  }
}

void BiquadCascade::runFloat(float* samples, size_t frames,
                             const FloatStage& stage,
                             FloatHistory& history) noexcept {
  // Locals, so nothing is reloaded after each store to the block
  const float a1 = stage.a1;
  const float a2 = stage.a2;
  const float a3 = stage.a3;
  const float m0 = stage.m0;
  const float m1 = stage.m1;
  const float m2 = stage.m2;
  float s1 = history.s1;
  float s2 = history.s2;

  // Trapezoidal state-variable filter: v1 is the band pass output, v2 the
  // low pass, and the two states are the integrators
  for (size_t i = 0; i < frames; i++) {
    const float v0 = samples[i];
    const float v3 = v0 - s2;
    const float v1 = a1 * s1 + a2 * v3;
    const float v2 = s2 + a2 * s1 + a3 * v3;
    s1 = 2.0F * v1 - s1;
    s2 = 2.0F * v2 - s2;
    samples[i] = m0 * v0 + m1 * v1 + m2 * v2;
  }

  history.s1 = settled(s1);
  history.s2 = settled(s2);
}

// ---------------------------------------------------------------------------
// Fixed kernel
// ---------------------------------------------------------------------------

// First-order sections run here too, with b2 and a2 at zero
void BiquadCascade::runSecondOrder(std::span<Sample> samples,
                                   const Coefficients& coefficients,
                                   History& history) noexcept {
  using namespace fixed;

  // Locals, so the loop keeps everything in registers
  Sample x1 = history.x1;
  Sample x2 = history.x2;
  Sample y1 = history.y1;
  Sample y2 = history.y2;
  const Coeff b0 = coefficients.b0;
  const Coeff b1 = coefficients.b1;
  const Coeff b2 = coefficients.b2;
  const Coeff a1 = coefficients.a1;
  const Coeff a2 = coefficients.a2;

  // Starts from what the previous sample could not represent
  Acc acc = Acc::fromRaw(history.remainder);

  for (Sample& sample : samples) {
    const Sample x0 = sample;

    acc = mac(acc, x0, b0);
    acc = mac(acc, x1, b1);
    acc = mac(acc, x2, b2);
    acc = mac(acc, y1, a1);
    acc = mac(acc, y2, a2);

    acc = clampAccTo<Sample::FRAC>(acc);
    const Sample y0 = narrowTrunc<Sample::FRAC>(acc);
    acc = fracBelow<Sample::FRAC>(acc);

    x2 = x1;
    x1 = x0;
    y2 = y1;
    y1 = y0;
    sample = y0;
  }

  history.x1 = x1;
  history.x2 = x2;
  history.y1 = y1;
  history.y2 = y2;
  history.remainder = static_cast<int32_t>(acc.raw);
}
