#include "pif_data.h"
#include <algorithm>
#include <fmt/core.h>

namespace hgps::input {

double PIFTable::get_pif_value(int age, core::Gender gender, int year_post_intervention) const {
    // Look for exact match first
    for (const auto& item : data_) {
        if (item.age == age && item.gender == gender && item.year_post_intervention == year_post_intervention) {
            return item.pif_value;
        }
    }
    
    // If no exact match, look for closest year_post_intervention
    double closest_pif = 0.0;
    int min_time_diff = std::numeric_limits<int>::max();
    
    for (const auto& item : data_) {
        if (item.age == age && item.gender == gender) {
            int time_diff = std::abs(item.year_post_intervention - year_post_intervention);
            if (time_diff < min_time_diff) {
                min_time_diff = time_diff;
                closest_pif = item.pif_value;
            }
        }
    }
    
    return closest_pif;
}

void PIFTable::add_item(const PIFDataItem& item) {
    data_.push_back(item);
}

void PIFData::add_scenario_data(const std::string& scenario, PIFTable&& table) {
    scenario_data_[scenario] = std::move(table);
}

const PIFTable* PIFData::get_scenario_data(const std::string& scenario) const {
    auto it = scenario_data_.find(scenario);
    if (it != scenario_data_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> PIFData::get_scenario_names() const {
    std::vector<std::string> names;
    names.reserve(scenario_data_.size());
    
    for (const auto& pair : scenario_data_) {
        names.push_back(pair.first);
    }
    
    return names;
}

} // namespace hgps::input
