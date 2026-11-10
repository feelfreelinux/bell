#pragma once

// Standard includes
#include <cstddef>
#include <optional>
#include <string>

// Library includes
#include "nonstd/expected.hpp"

namespace bell::dsp {

/**
 * @brief Why a pipeline description was rejected
 */
struct Error {
  // Index of the offending node in the spec, if the problem is in one node
  std::optional<size_t> node;
  std::string message;

  // "node 3: frequency must be below half the sample rate"
  std::string describe() const {
    return node ? "node " + std::to_string(*node) + ": " + message : message;
  }
};

template <typename T>
using Expected = nonstd::expected<T, Error>;

inline nonstd::unexpected_type<Error> fail(std::string message) {
  return nonstd::make_unexpected(Error{std::nullopt, std::move(message)});
}

}  // namespace bell::dsp
