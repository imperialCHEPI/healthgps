#pragma once

#include <tuple>

namespace hgps::core {

/// @brief Population mortality trends for a country data structure
struct MortalityItem {
    /// @brief The country unique identifier code
    int location_id{};

    /// @brief Item reference time
    int at_time{};

    /// @brief Item reference time
    int with_age{};

    /// @brief Number of male deaths
    float males{};

    /// @brief Number of female deaths
    float females{};

    /// @brief Total number of deaths
    float total{};
};

/// @brief Determine whether a specified MortalityItem is less than another instance.
/// @param lhs The first instance to compare.
/// @param rhs The second instance to compare.
/// @return true if left instance is less than right instance; otherwise, false.
inline bool operator<(MortalityItem const &lhs, MortalityItem const &rhs) {
    return std::tie(lhs.at_time, lhs.with_age) < std::tie(rhs.at_time, rhs.with_age);
}

/// @brief Determine whether a specified MortalityItem is greater than another instance.
/// @param lhs The first instance to compare.
/// @param rhs The second instance to compare.
/// @return true if left instance is greater than right instance; otherwise, false.
inline bool operator>(MortalityItem const &lhs, MortalityItem const &rhs) {
    return std::tie(lhs.at_time, lhs.with_age) > std::tie(rhs.at_time, rhs.with_age);
}
} // namespace hgps::core
