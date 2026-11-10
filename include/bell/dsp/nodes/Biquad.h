#pragma once

// Standard includes
#include <complex>
#include <memory_resource>
#include <span>
#include <vector>

// Bell includes
#include "bell/dsp/Block.h"
#include "bell/dsp/Error.h"
#include "bell/dsp/Pipeline.h"
#include "bell/dsp/Spec.h"

/**
 * @file Biquad.h
 * @brief IIR filtering: design in double precision, and the node that runs it
 */

namespace bell::dsp {

/**
 * @brief One first or second order filter section
 *
 * H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
 */
struct Section {
  double b0 = 1.0;
  double b1 = 0.0;
  double b2 = 0.0;
  double a1 = 0.0;
  double a2 = 0.0;

  bool isFirstOrder() const { return b2 == 0.0 && a2 == 0.0; }

  // True if the section changes nothing, as a 0 dB peaking filter does
  bool isIdentity() const;

  // Response at `omega` radians per sample, pi is half the sample rate
  std::complex<double> response(double omega) const;

  // True if the fixed kernel can run the section as it is: the coefficients
  // fit their format and cannot overflow the accumulator
  bool fitsFixedPoint() const;

  // The same filter with its output scaled by `gain`
  Section scaled(double gain) const {
    return {b0 * gain, b1 * gain, b2 * gain, a1, a2};
  }

  bool operator==(const Section&) const = default;
};

// The one second-order section equal to two first-order ones in series
Section combineFirstOrder(const Section& first, const Section& second);

// Filter design. Each appends the sections for the filter at the given sample
// rate, or says which parameter is unusable

Expected<void> design(const spec::Peaking& filter, double sampleRate,
                      std::vector<Section>& out);
Expected<void> design(const spec::LowShelf& filter, double sampleRate,
                      std::vector<Section>& out);
Expected<void> design(const spec::HighShelf& filter, double sampleRate,
                      std::vector<Section>& out);
Expected<void> design(const spec::LowPass& filter, double sampleRate,
                      std::vector<Section>& out);
Expected<void> design(const spec::HighPass& filter, double sampleRate,
                      std::vector<Section>& out);
Expected<void> design(const spec::BandPass& filter, double sampleRate,
                      std::vector<Section>& out);
Expected<void> design(const spec::Notch& filter, double sampleRate,
                      std::vector<Section>& out);
Expected<void> design(const spec::AllPass& filter, double sampleRate,
                      std::vector<Section>& out);
Expected<void> design(const spec::Biquad& filter, double sampleRate,
                      std::vector<Section>& out);

/**
 * @brief Runs a list of sections, one after another, on each of its channels
 *
 * Channels share the coefficients and each keeps its own history. Both
 * kernels saturate instead of wrapping, so an overdriven filter recovers.
 */
class BiquadCascade final : public Node {
 public:
  // Sections must be stable and, for Arithmetic::Fixed, fitsFixedPoint()
  BiquadCascade(ChannelMask channels, std::span<const Section> sections,
                std::pmr::memory_resource* memory,
                Arithmetic arithmetic = defaultArithmetic());

  void process(Block& block, const Context& context) noexcept override;
  void reset() noexcept override;

 private:
  struct Coefficients {
    // Feedback coefficients are stored negated, so the kernel only adds
    fixed::Coeff b0, b1, b2, a1, a2;
  };

  struct History {
    Sample x1, x2, y1, y2;
    // Accumulator bits below the sample format, carried to the next sample
    int32_t remainder;
  };

  static void runSecondOrder(std::span<Sample> samples,
                             const Coefficients& coefficients,
                             History& history) noexcept;

  // The same section as a state-variable filter: the loop coefficients, then
  // how its outputs mix into the result
  struct FloatStage {
    float a1, a2, a3, m0, m1, m2;
  };

  struct FloatHistory {
    float s1, s2;
  };

  static FloatStage toFloatStage(const Section& section);
  static void runFloat(float* samples, size_t frames, const FloatStage& stage,
                       FloatHistory& history) noexcept;

  void processFixed(Block& block) noexcept;
  void processFloat(Block& block) noexcept;

  ChannelMask channels;
  Arithmetic arithmetic;

  // Only the set for the chosen arithmetic is filled in
  std::pmr::vector<Coefficients> stages;
  // One History per channel per stage, channel-major
  std::pmr::vector<History> histories;

  std::pmr::vector<FloatStage> floatStages;
  std::pmr::vector<FloatHistory> floatHistories;
  // One channel of the block as floats, while its stages run
  std::pmr::vector<float> scratch;
};

}  // namespace bell::dsp
