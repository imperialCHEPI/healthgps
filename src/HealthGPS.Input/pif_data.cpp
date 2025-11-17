#include "pif_data.h"
#include <algorithm>
#include <fmt/core.h>

namespace hgps::input {

double PIFTable::get_pif_value(int age, core::Gender gender, int year_post_intervention) const {
    // OPTIMIZATION: Direct array access - no lookups, no searching
    //  Formula: index = ((year - min_year) * age_range * GENDERS) + (gender_index * age_range) +
    //  (age - min_age) Gender mapping: male=0, female=1 (convert from enum: male=1->0, female=2->1)
    //
    // NOTE: PIF data may contain ages 0-110, but the simulation may be configured for a smaller
    // age range (e.g., 0-100). The bounds checks below ensure safe access even when the simulation
    // age range is smaller than the PIF data range.

    // Early bounds check for performance (checks against PIF data's actual age range)
    if (age < min_age_ || age > max_age_ || year_post_intervention < min_year_ ||
        year_post_intervention > max_year_) {
        if (age > max_age_) {
            std::fprintf(stderr, "[PIF] get_pif_value: age %d > max_age_ %d - returning 0.0\n", age,
                         max_age_);
            std::fflush(stderr);
        }
        return 0.0;
    }

    constexpr int GENDERS = 2; // male, female
    int gender_index = (gender == core::Gender::male) ? 0 : 1;
    int index = ((year_post_intervention - min_year_) * age_range_ * GENDERS) +
                (gender_index * age_range_) + (age - min_age_);

    // Additional bounds check on calculated index to prevent out-of-range access
    // This is a critical safety check that prevents crashes if there's any mismatch between
    // the calculated index and the actual array size (e.g., due to data inconsistencies or
    // edge cases in index calculation)
    if (index < 0 || static_cast<std::size_t>(index) >= direct_array_.size()) {
        std::fprintf(stderr,
                     "[CRASH LOCATION] pif_data.cpp:31 - index %d >= direct_array_.size() %zu "
                     "(age=%d, min_age=%d, max_age=%d, age_range=%d)\n",
                     index, direct_array_.size(), age, min_age_, max_age_, age_range_);
        std::fflush(stderr);
        return 0.0;
    }

    // Direct access with bounds validation
    return direct_array_[index].pif_value;
}

void PIFTable::add_item(const PIFDataItem &item) { data_.push_back(item); }

void PIFTable::build_hash_table() {
    // OPTIMIZATION: Build direct array for ultra-fast access
    // Calculate dynamic ranges from actual data

    if (data_.empty()) {
        direct_array_.clear();
        return;
    }

    // Find actual data ranges
    int min_age = data_[0].age;
    int max_age = data_[0].age;
    int min_year = data_[0].year_post_intervention;
    int max_year = data_[0].year_post_intervention;

    for (const auto &item : data_) {
        min_age = std::min(min_age, item.age);
        max_age = std::max(max_age, item.age);
        min_year = std::min(min_year, item.year_post_intervention);
        max_year = std::max(max_year, item.year_post_intervention);
    }

    // Store ranges for use in get_pif_value
    min_age_ = min_age; // 0
    max_age_ = max_age; // 110
    min_year_ = min_year;
    max_year_ = max_year;
    age_range_ = max_age - min_age + 1;
    year_range_ = max_year - min_year + 1; // e.g. 0-30 years = 31

    constexpr int GENDERS = 2; // male, female
    int array_size = year_range_ * age_range_ * GENDERS;

    direct_array_.resize(array_size);

    // Populate with actual data using dynamic ranges
    for (const auto &item : data_) {
        int gender_index = (item.gender == core::Gender::male) ? 0 : 1;
        int index = ((item.year_post_intervention - min_year_) * age_range_ * GENDERS) +
                    (gender_index * age_range_) + (item.age - min_age_);

        if (index >= 0 && index < array_size) {
            direct_array_[index] = item;
        }
    }
}

void PIFData::add_scenario_data(const std::string &scenario, PIFTable &&table) {
    scenario_data_[scenario] = std::move(table);
}

const PIFTable *PIFData::get_scenario_data(const std::string &scenario) const {
    auto it = scenario_data_.find(scenario);
    if (it != scenario_data_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> PIFData::get_scenario_names() const {
    std::vector<std::string> names;
    names.reserve(scenario_data_.size());

    for (const auto &pair : scenario_data_) {
        names.push_back(pair.first);
    }

    return names;
}

} // namespace hgps::input
