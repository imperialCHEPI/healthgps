#pragma once
#include "HealthGPS.Core/array2d.h"
#include "HealthGPS.Core/identifier.h"
#include "gender_table.h"
#include "monotonic_vector.h"
#include <map>

namespace hgps {

/// @brief Defines the relative risk factors lookup data type
///
/// @note When a lookup value does not exists at the exact
/// breakpoint values provided, a data at bound value or a
/// linear interpolated value is returned for the operation.
class RelativeRiskLookup {
  public:
    RelativeRiskLookup() = delete;

    /// @brief Initialises a new instance of the RelativeRiskLookup class.
    /// @param rows The rows lookup breakpoints
    /// @param cols The columns lookup breakpoints
    /// @param values The lookup data values
    /// @throws std::out_of_range for lookup breakpoints and values size mismatch.
    RelativeRiskLookup(const MonotonicVector<int> &rows, const MonotonicVector<float> &cols,
                       core::FloatArray2D &&values);

    /// @brief Gets the number of elements in the lookup dataset
    /// @return Number of lookup elements.
    std::size_t size() const noexcept;

    /// @brief Gets the number of rows lookup breakpoints
    /// @return Number of rows
    std::size_t rows() const noexcept;

    /// @brief Gets the number of columns lookup breakpoints
    /// @return Number of columns
    std::size_t columns() const noexcept;

    /// @brief Checks if the lookup has no data.
    /// @return true if the lookup data is empty; otherwise, false.
    bool empty() const noexcept;

    /// @brief Lookup a value at specific breakpoints with bounds checking
    /// @param age The row value
    /// @param value The column value
    /// @return The lookup value
    /// @throws std::out_of_range for row breakpoint value outside of the bounds.
    float at(const int age, const float value) const;

    /// @brief Lookup a value at specific breakpoints with bounds checking
    /// @param age The row value
    /// @param value The column value
    /// @return The lookup value
    /// @throws std::out_of_range for row breakpoint value outside of the bounds.
    float operator()(const int age, const float value) const;

    /// @brief True when the table has a row for this age.
    /// @param age Row breakpoint to test.
    /// @return true when age is a row in the table.
    bool contains_age(const int age) const noexcept;

    /// @brief Checks if the lookup contains a value with specific breakpoints
    /// @param age The row value
    /// @param value The column value
    /// @return true if there is such a value; otherwise, false.
    bool contains(const int age, const float value) const noexcept;

  private:
    core::FloatArray2D table_;
    std::map<int, int> rows_index_;
    std::map<float, int> cols_index_;

    float lookup_value(const int age, const float value) const;
};

/// @brief Defines the relative risk factor table type
using RelativeRiskTableMap = std::map<core::Identifier, FloatAgeGenderTable>;

/// @brief Defines the relative risk factor lookup type
using RelativeRiskLookupMap =
    std::map<core::Identifier, std::map<core::Gender, RelativeRiskLookup>>;

/// @brief Read one relative-risk cell, or throw a message that names the factor, age, and sex.
/// @param lookup Sex-specific relative-risk table.
/// @param factor Risk-factor key (for example BMI).
/// @param age Person age.
/// @param gender Person sex.
/// @param value Risk-factor value used for the column lookup.
/// @return Relative risk multiplier.
/// @throws std::out_of_range when the table has no row for age, or the table is empty.
float require_relative_risk(const RelativeRiskLookup &lookup, const core::Identifier &factor,
                            int age, core::Gender gender, float value);

/// @brief Defines the relative risk factors data type
struct RelativeRisk {
    /// @brief Disease to diseases relative risk values
    RelativeRiskTableMap diseases;

    /// @brief Risk factor to diseases relative risk values
    RelativeRiskLookupMap risk_factors;
};
} // namespace hgps
