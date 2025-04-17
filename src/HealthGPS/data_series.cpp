#include "data_series.h"
#include "HealthGPS.Core/string_util.h"

#include "fmt/core.h"
#include <stdexcept>
#include <iostream>

namespace hgps {
DataSeries::DataSeries(std::size_t sample_size) : sample_size_{sample_size} {
    data_.emplace(core::Gender::male, std::map<std::string, std::vector<double>>{});
    data_.emplace(core::Gender::female, std::map<std::string, std::vector<double>>{});
}

std::vector<double> &DataSeries::operator()(core::Gender gender, const std::string &key) {
    if (data_.find(gender) == data_.end()) {
        std::cout << "\nERROR: Gender not found in data_ map: " << static_cast<int>(gender);
        throw std::out_of_range("Gender not found in data map");
    }
    
    auto& gender_map = data_.at(gender);
    if (gender_map.find(key) == gender_map.end()) {
        std::cout << "\nERROR: Key not found in gender map: " << key;
        throw std::out_of_range("Key not found in gender map: " + key);
    }
    
    return data_.at(gender).at(key);
}

std::vector<double> &DataSeries::at(core::Gender gender, const std::string &key) {
    return data_.at(gender).at(key);
}

const std::vector<double> &DataSeries::at(core::Gender gender, const std::string &key) const {
    return data_.at(gender).at(key);
}

const std::vector<std::string> &DataSeries::channels() const noexcept { return channels_; }

void DataSeries::add_channel(std::string key) {
    auto channel_key = core::to_lower(key);
    if (core::case_insensitive::contains(channels_, channel_key)) {
        throw std::logic_error(fmt::format("No duplicated channel key: {} is not allowed.", key));
    }

    channels_.emplace_back(channel_key);
    data_.at(core::Gender::male).emplace(channel_key, std::vector<double>(sample_size_));
    data_.at(core::Gender::female).emplace(channel_key, std::vector<double>(sample_size_));
}

void DataSeries::add_channels(const std::vector<std::string> &keys) {
    for (const auto &item : keys) {
        add_channel(item);
    }
}

std::size_t DataSeries::size() const noexcept { return channels_.size(); }

std::size_t DataSeries::sample_size() const noexcept { return sample_size_; }

bool DataSeries::ensure_age_exists(int age) {
    bool created = false;
    
    // Convert age to string for key lookup
    auto age_key = std::to_string(age);
    
    // Check and initialize for male gender
    auto& male_data = data_.at(core::Gender::male);
    if (male_data.find(age_key) == male_data.end()) {
        male_data.emplace(age_key, std::vector<double>(sample_size_));
        created = true;
    }
    
    // Check and initialize for female gender
    auto& female_data = data_.at(core::Gender::female);
    if (female_data.find(age_key) == female_data.end()) {
        female_data.emplace(age_key, std::vector<double>(sample_size_));
        created = true;
    }
    
    return created;
}
} // namespace hgps
