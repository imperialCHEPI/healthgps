#include "relative_risk.h"

#include <fmt/format.h>

namespace hgps {
namespace {

const char *sex_label(core::Gender gender) noexcept {
    switch (gender) {
    case core::Gender::male:
        return "male";
    case core::Gender::female:
        return "female";
    default:
        return "unknown";
    }
}

} // namespace

RelativeRiskLookup::RelativeRiskLookup(const MonotonicVector<int> &rows,
                                       const MonotonicVector<float> &cols,
                                       core::FloatArray2D &&values)
    : table_{std::move(values)} {
    if (rows.size() != table_.rows() || cols.size() != table_.columns()) {
        throw std::out_of_range("Lookup breakpoints and values size mismatch.");
    }

    auto rows_count = static_cast<int>(rows.size());
    for (auto index = 0; index < rows_count; index++) {
        rows_index_.emplace(rows[index], index);
    }

    auto cols_count = static_cast<int>(cols.size());
    for (auto index = 0; index < cols_count; index++) {
        cols_index_.emplace(cols[index], index);
    }
}

std::size_t RelativeRiskLookup::size() const noexcept { return table_.size(); }

std::size_t RelativeRiskLookup::rows() const noexcept { return table_.rows(); }

std::size_t RelativeRiskLookup::columns() const noexcept { return table_.columns(); }

bool RelativeRiskLookup::empty() const noexcept {
    return rows_index_.empty() || cols_index_.empty();
}

float RelativeRiskLookup::at(const int age, const float value) const {
    return lookup_value(age, value);
}

float RelativeRiskLookup::operator()(const int age, const float value) const {
    return lookup_value(age, value);
}

bool RelativeRiskLookup::contains_age(const int age) const noexcept {
    return rows_index_.contains(age);
}

bool RelativeRiskLookup::contains(const int age, const float value) const noexcept {
    if (rows_index_.contains(age)) {
        return cols_index_.contains(value);
    }

    return false;
}

float require_relative_risk(const RelativeRiskLookup &lookup, const core::Identifier &factor,
                            int age, core::Gender gender, float value) {
    // MAHIMA: Name the factor here. map::at only says "invalid map key", which hides BMI/age.
    if (lookup.empty() || !lookup.contains_age(age)) {
        throw std::out_of_range(fmt::format(
            "Relative risk for factor '{}' has no row for age {} ({}). Factor value was {}.",
            factor.to_string(), age, sex_label(gender), value));
    }
    return lookup(age, value);
}

float RelativeRiskLookup::lookup_value(const int age, const float value) const {
    if (empty()) {
        throw std::out_of_range(
            "Relative risk lookup table is empty, so no age or value can be resolved.");
    }

    const auto row_it = rows_index_.find(age);
    if (row_it == rows_index_.end()) {
        throw std::out_of_range(
            fmt::format("Relative risk lookup has no row for age {} (table ages {} to {}).", age,
                        rows_index_.begin()->first, rows_index_.rbegin()->first));
    }

    const auto row_index = row_it->second;
    if (value <= cols_index_.begin()->first) {
        return table_(row_index, cols_index_.begin()->second);
    }

    if (value >= cols_index_.rbegin()->first) {
        return table_(row_index, cols_index_.rbegin()->second);
    }

    if (cols_index_.contains(value)) {
        return table_(row_index, cols_index_.at(value));
    }

    auto it = cols_index_.lower_bound(value);
    auto it_prev = std::prev(it);
    auto x1 = it_prev->first;
    auto x2 = it->first;
    auto y1 = table_(row_index, it_prev->second);
    auto y2 = table_(row_index, it->second);

    // Linear interpolation
    return (y2 - y1) * (value - x1) / (x2 - x1) + y1;
}
} // namespace hgps
