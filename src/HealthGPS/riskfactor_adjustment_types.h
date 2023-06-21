#pragma once
#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include "map2d.h"
#include <vector>

namespace hgps {

/// @brief Defines the risk factors adjustment table type
using FactorAdjustmentTable = Map2d<core::Gender, core::Identifier, std::vector<double>>;

/// @brief Defines the risk factor baseline adjustment data type
struct BaselineAdjustment final {
    /// @brief Initialises a new instance of the BaselineAdjustment structure
    BaselineAdjustment() = default;

    /// @brief Initialises a new instance of the BaselineAdjustment structure
    /// @param adjustment_table The baseline adjustment table
    /// @throws std::invalid_argument for empty adjustment table or table missing a gender entry
    BaselineAdjustment(FactorAdjustmentTable &&adjustment_table)
        : values{std::move(adjustment_table)} {
        if (values.empty()) {
            throw std::invalid_argument("The risk factors adjustment table must not be empty.");
        } else if (values.rows() != 2) {
            throw std::out_of_range(
                "The risk factors adjustment definition must contain two tables.");
        }

        if (!values.contains(core::Gender::male)) {
            throw std::invalid_argument("Missing the required adjustment table for male.");
        } else if (!values.contains(core::Gender::female)) {
            throw std::invalid_argument("Missing the required adjustment table for female.");
        }
    }

    /// @brief The risk factors adjustment table values
    FactorAdjustmentTable values{};
};

/// @brief Defines the first statistical moment type
struct FirstMoment {
    /// @brief Determine whether the moment is empty
    /// @return true if the moment is empty; otherwise, false.
    bool empty() const noexcept { return count_ < 1; }
    /// @brief Gets the number of values added to the moment
    /// @return Number of values
    int count() const noexcept { return count_; }
    /// @brief Gets the values sum
    /// @return Sum value
    double sum() const noexcept { return sum_; }

    /// @brief Gets the values mean
    /// @return Mean value
    double mean() const noexcept {
        if (count_ > 0) {
            return sum_ / count_;
        }

        return 0.0;
    }

    /// @brief Appends a value to the moment
    /// @param value The new value
    void append(double value) noexcept {
        count_++;
        sum_ += value;
    }

    /// @brief Clear the moment
    void clear() noexcept {
        count_ = 0;
        sum_ = 0.0;
    }

    /// @brief Compare FirstMoment instances
    /// @param other The other instance to compare
    /// @return The comparison result
    auto operator<=>(const FirstMoment &other) const = default;

  private:
    int count_{};
    double sum_{};
};
} // namespace hgps
