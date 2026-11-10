#pragma once

// Standard includes
#include <cstdint>
#include <limits>
#include <type_traits>

/**
 * @file Fixed.h
 * @brief Fixed point values that carry their Q format in their type
 *
 * Shifts are derived from the operand formats, so mixing formats does not
 * compile. No implicit conversions, multiplies and narrowing truncate.
 */

namespace bell::dsp::fixed {

namespace detail {
// Exact power of two as a double
constexpr double pow2d(int exponent) {
  double result = 1.0;
  for (int i = 0; i < exponent; i++) {
    result *= 2.0;
  }
  for (int i = 0; i > exponent; i--) {
    result *= 0.5;
  }
  return result;
}
}  // namespace detail

/**
 * @brief A signed fixed point value, `raw / 2^FRAC_BITS`
 */
template <int FRAC_BITS, typename Raw = int32_t>
struct Fixed {
  static_assert(std::is_integral_v<Raw> && std::is_signed_v<Raw>,
                "Fixed requires a signed integral storage type");
  static_assert(FRAC_BITS >= 0 && FRAC_BITS < static_cast<int>(sizeof(Raw) * 8),
                "FRAC_BITS must leave at least a sign bit");

  static constexpr int FRAC = FRAC_BITS;
  static constexpr int BITS = static_cast<int>(sizeof(Raw) * 8);

  static constexpr Raw RAW_MAX = std::numeric_limits<Raw>::max();
  static constexpr Raw RAW_MIN = std::numeric_limits<Raw>::min();

  Raw raw;

  // Wraps an already-scaled integer, nothing is converted
  static constexpr Fixed fromRaw(Raw value) { return Fixed{value}; }

  static constexpr Fixed zero() { return Fixed{0}; }

  // The value 1.0, if the format can hold it
  static constexpr Fixed one() {
    static_assert(FRAC < BITS - 1, "1.0 is not representable in this format");
    return Fixed{static_cast<Raw>(Raw{1} << FRAC)};
  }

  /**
   * @brief Convert from a real value, rounding half away from zero
   *
   * Saturates, as an overflowing float to integer cast is undefined and
   * platforms disagree on it. Control thread only.
   */
  static constexpr Fixed fromDouble(double value) {
    const double scaled = value * detail::pow2d(FRAC);
    const double rounded = (scaled >= 0.0) ? scaled + 0.5 : scaled - 0.5;

    // RAW_MAX + 1, which a double holds exactly even for 64-bit storage
    constexpr double RAW_LIMIT = detail::pow2d(BITS - 1);

    if (rounded >= RAW_LIMIT) {
      return Fixed{RAW_MAX};
    }
    if (rounded < -RAW_LIMIT) {
      return Fixed{RAW_MIN};
    }
    return Fixed{static_cast<Raw>(rounded)};
  }

  // Exact for storage up to 53 significant bits
  constexpr double toDouble() const {
    return static_cast<double>(raw) * detail::pow2d(-FRAC);
  }
};

template <int F, typename R>
constexpr bool operator==(Fixed<F, R> lhs, Fixed<F, R> rhs) {
  return lhs.raw == rhs.raw;
}

/**
 * @brief Multiply two values into an explicitly chosen format, truncating
 *
 * Wraps if the result does not fit, use mulToSat() for unbounded inputs.
 */
template <int TO_FRAC, int FA, int FB>
constexpr Fixed<TO_FRAC> mulTo(Fixed<FA> lhs, Fixed<FB> rhs) {
  constexpr int SHIFT = FA + FB - TO_FRAC;
  static_assert(SHIFT >= 0,
                "target format has more fractional bits than the product");
  const int64_t shifted =
      (static_cast<int64_t>(lhs.raw) * static_cast<int64_t>(rhs.raw)) >> SHIFT;
  return Fixed<TO_FRAC>::fromRaw(static_cast<int32_t>(shifted));
}

// As mulTo(), saturating instead of wrapping
template <int TO_FRAC, int FA, int FB>
constexpr Fixed<TO_FRAC> mulToSat(Fixed<FA> lhs, Fixed<FB> rhs) {
  constexpr int SHIFT = FA + FB - TO_FRAC;
  static_assert(SHIFT >= 0,
                "target format has more fractional bits than the product");
  using Target = Fixed<TO_FRAC>;

  const int64_t shifted =
      (static_cast<int64_t>(lhs.raw) * static_cast<int64_t>(rhs.raw)) >> SHIFT;

  // It fits when narrowing changes nothing, one comparison instead of two
  // 64-bit range checks
  const auto narrowed = static_cast<int32_t>(shifted);
  if (narrowed != shifted) [[unlikely]] {
    return Target::fromRaw(shifted < 0 ? Target::RAW_MIN : Target::RAW_MAX);
  }
  return Target::fromRaw(narrowed);
}

/**
 * @brief Multiply-accumulate into a wide accumulator, exact
 *
 * The accumulator's format must be the sum of the operand formats.
 */
template <int FACC, int FA, int FB>
constexpr Fixed<FACC, int64_t> mac(Fixed<FACC, int64_t> acc, Fixed<FA> lhs,
                                   Fixed<FB> rhs) {
  static_assert(FACC == FA + FB,
                "accumulator format must equal the sum of the operand formats");
  return Fixed<FACC, int64_t>::fromRaw(
      acc.raw + static_cast<int64_t>(lhs.raw) * static_cast<int64_t>(rhs.raw));
}

/**
 * @brief Narrow an accumulator to a 32-bit format, truncating
 *
 * Wraps outside the target's range, clampAccTo() first rules that out.
 */
template <int TO_FRAC, int FACC>
constexpr Fixed<TO_FRAC> narrowTrunc(Fixed<FACC, int64_t> acc) {
  constexpr int SHIFT = FACC - TO_FRAC;
  static_assert(SHIFT >= 0, "narrowing must not increase fractional bits");
  return Fixed<TO_FRAC>::fromRaw(static_cast<int32_t>(acc.raw >> SHIFT));
}

// Narrows an accumulator to a 32-bit format, saturating
template <int TO_FRAC, int FACC>
constexpr Fixed<TO_FRAC> narrowSat(Fixed<FACC, int64_t> acc) {
  constexpr int SHIFT = FACC - TO_FRAC;
  static_assert(SHIFT >= 0, "narrowing must not increase fractional bits");
  using Target = Fixed<TO_FRAC>;

  const int64_t shifted = acc.raw >> SHIFT;
  // One comparison on the common path, see mulToSat()
  const auto narrowed = static_cast<int32_t>(shifted);
  if (narrowed != shifted) [[unlikely]] {
    return Target::fromRaw(shifted < 0 ? Target::RAW_MIN : Target::RAW_MAX);
  }
  return Target::fromRaw(narrowed);
}

/**
 * @brief The part of an accumulator below the target format's LSB
 *
 * Carried into the next accumulation it is error feedback. Never negative.
 */
template <int TO_FRAC, int FACC>
constexpr Fixed<FACC, int64_t> fracBelow(Fixed<FACC, int64_t> acc) {
  constexpr int SHIFT = FACC - TO_FRAC;
  static_assert(SHIFT >= 0, "no fractional bits below the target format");
  static_assert(SHIFT < 63, "shift out of range");
  constexpr int64_t MASK = (int64_t{1} << SHIFT) - 1;
  return Fixed<FACC, int64_t>::fromRaw(acc.raw & MASK);
}

/**
 * @brief Clamp an accumulator to the range a target format can hold
 *
 * Bits below the target's LSB are kept, so fracBelow() still matches.
 */
template <int TO_FRAC, int FACC>
constexpr Fixed<FACC, int64_t> clampAccTo(Fixed<FACC, int64_t> acc) {
  constexpr int SHIFT = FACC - TO_FRAC;
  static_assert(SHIFT >= 0, "target format is wider than the accumulator");
  using Acc = Fixed<FACC, int64_t>;
  using Target = Fixed<TO_FRAC>;

  constexpr int64_t MASK = (int64_t{1} << SHIFT) - 1;
  constexpr int64_t UPPER =
      (static_cast<int64_t>(Target::RAW_MAX) << SHIFT) | MASK;
  constexpr int64_t LOWER = static_cast<int64_t>(Target::RAW_MIN) << SHIFT;

  if (acc.raw > UPPER) {
    return Acc::fromRaw(UPPER);
  }
  if (acc.raw < LOWER) {
    return Acc::fromRaw(LOWER);
  }
  return acc;
}

// ---------------------------------------------------------------------------
// Pipeline formats
// ---------------------------------------------------------------------------

// Audio samples, range +-16.0. PCM full scale is +-1.0, leaving 24 dB of
// headroom inside the pipeline
using Sample = Fixed<27>;

// Biquad coefficients, range +-8.0
using Coeff = Fixed<28>;

// Gain factors, range +-128.0, so a boost above +6 dB fits
using Gain = Fixed<24>;

// Filter accumulator, range +-256.0. Cannot overflow while a section's
// absolute coefficients sum to less than 16, see Section::fitsFixedPoint()
using Acc = Fixed<Sample::FRAC + Coeff::FRAC, int64_t>;

// ---------------------------------------------------------------------------
// PCM conversion
// ---------------------------------------------------------------------------

// From a 16-bit PCM sample, exact
constexpr Sample fromS16(int16_t value) {
  constexpr int SHIFT = Sample::FRAC - 15;
  static_assert(SHIFT >= 0, "sample format too narrow for 16-bit PCM");
  return Sample::fromRaw(static_cast<int32_t>(value) << SHIFT);
}

// From a 32-bit PCM sample, dropping the low bits that do not fit
constexpr Sample fromS32(int32_t value) {
  constexpr int SHIFT = 31 - Sample::FRAC;
  static_assert(SHIFT >= 0, "sample format too wide for 32-bit PCM");
  return Sample::fromRaw(value >> SHIFT);
}

// From a 24-bit PCM sample held in the low bits of an int32
constexpr Sample fromS24(int32_t value) {
  constexpr int SHIFT = Sample::FRAC - 23;
  static_assert(SHIFT >= 0, "sample format too narrow for 24-bit PCM");
  return Sample::fromRaw(value << SHIFT);
}

// To 16-bit PCM, rounding to nearest and saturating
constexpr int16_t toS16SatRound(Sample value) {
  constexpr int SHIFT = Sample::FRAC - 15;
  // floor((raw + half) / 2^SHIFT), written so it cannot overflow 32 bits
  const int32_t shifted =
      (value.raw >> SHIFT) + ((value.raw >> (SHIFT - 1)) & 1);
  if (shifted > 32767) {
    return 32767;
  }
  if (shifted < -32768) {
    return -32768;
  }
  return static_cast<int16_t>(shifted);
}

// To 24-bit PCM in the low bits of an int32, rounding and saturating
constexpr int32_t toS24SatRound(Sample value) {
  constexpr int SHIFT = Sample::FRAC - 23;
  const int32_t shifted =
      (value.raw >> SHIFT) + ((value.raw >> (SHIFT - 1)) & 1);
  if (shifted > 8388607) {
    return 8388607;
  }
  if (shifted < -8388608) {
    return -8388608;
  }
  return shifted;
}

// To 32-bit PCM, saturating
constexpr int32_t toS32Sat(Sample value) {
  constexpr int SHIFT = 31 - Sample::FRAC;
  const int64_t shifted = static_cast<int64_t>(value.raw) << SHIFT;
  if (shifted > std::numeric_limits<int32_t>::max()) {
    return std::numeric_limits<int32_t>::max();
  }
  if (shifted < std::numeric_limits<int32_t>::min()) {
    return std::numeric_limits<int32_t>::min();
  }
  return static_cast<int32_t>(shifted);
}

}  // namespace bell::dsp::fixed
