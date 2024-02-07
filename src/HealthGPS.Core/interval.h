#pragma once

#include "HealthGPS.Core/exception.h"
#include "forward_type.h"
#include "string_util.h"

#include <algorithm>
#include <fmt/format.h>

namespace hgps::core {

/// @brief Numeric interval representation data type
/// @tparam TYPE The numerical type
template <Numerical TYPE> class Interval {
  public:
    /// @brief Initialises a new instance of the Interval class
    Interval() = default;

    /// @brief @brief Initialises a new instance of the Interval class
    /// @param lower_value Lower bound value
    /// @param upper_value Upper bound value
    explicit Interval(TYPE lower_value, TYPE upper_value)
        : lower_{lower_value}, upper_{upper_value} {
        if (lower_ > upper_) {
            throw HgpsException(fmt::format("Invalid interval: {}-{}", lower_, upper_));
        }
    }

    /// @brief Gets the interval lower bound
    /// @return The lower bound value
    TYPE lower() const noexcept { return lower_; }

    /// @brief Gets the interval upper bound
    /// @return The upper bound value
    TYPE upper() const noexcept { return upper_; }

    /// @brief Gets the interval length
    /// @return Length of the interval
    TYPE length() const noexcept { return upper_ - lower_; }

    /// @brief Determines whether a value is in the Interval.
    /// @param value The value to check
    /// @return true if the value is in the interval; otherwise, false.
    bool contains(TYPE value) const noexcept { return lower_ <= value && value <= upper_; }

    /// @brief Determines whether an Interval is inside this instance interval.
    /// @param other The other Interval to check
    /// @return true if the other Interval is inside this instance; otherwise, false.
    bool contains(Interval<TYPE> &other) const noexcept {
        return contains(other.lower_) && contains(other.upper_);
    }

    /// @brief Clamp a given value to the interval boundaries
    /// @param value The value to clamp
    /// @return The clamped value
    TYPE clamp(TYPE value) const noexcept { return std::clamp(value, lower_, upper_); }

    /// @brief Convert this instance to a string representation
    /// @return The equivalent string representation
    std::string to_string() const noexcept { return fmt::format("{}-{}", lower_, upper_); }

    /// @brief Compare two Interval instances
    /// @param rhs The Interval to compare to this instance.
    /// @return The comparison result
    auto operator<=>(const Interval<TYPE> &rhs) const = default;

  private:
    TYPE lower_{};
    TYPE upper_{};
};

/// @brief Interval representation integer data type
using IntegerInterval = Interval<int>;

/// @brief Interval representation for single-precision data type
using FloatInterval = Interval<float>;

/// @brief Interval representation for double-precision data type
using DoubleInterval = Interval<double>;

/// @brief Converts the string representation to its IntegerInterval equivalent.
/// @param value The string representation to parse.
/// @param delims The fields delimiter to use.
/// @return The new IntegerInterval instance.
/// @throws std::invalid_argument for invalid integer interval string representation formats.
IntegerInterval parse_integer_interval(const std::string_view &value,
                                       const std::string_view delims = "-");

/// @brief Converts the string representation to its FloatInterval equivalent.
/// @param value The string representation to parse.
/// @param delims The fields delimiter to use.
/// @return The new FloatInterval instance.
/// @throws std::invalid_argument for invalid integer interval string representation formats.
FloatInterval parse_float_interval(const std::string_view &value,
                                   const std::string_view delims = "-");

/// @brief Converts the string representation to its DoubleInterval equivalent.
/// @param value The string representation to parse.
/// @param delims The fields delimiter to use.
/// @return The new DoubleInterval instance.
/// @throws std::invalid_argument for invalid integer interval string representation formats.
DoubleInterval parse_double_interval(const std::string_view &value,
                                     const std::string_view delims = "-");
} // namespace hgps::core
