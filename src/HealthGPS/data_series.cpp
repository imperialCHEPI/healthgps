#include "data_series.h"
#include "HealthGPS.Core/string_util.h"

#include "fmt/core.h"
#include <array>    // Added for std::array
#include <iostream> // Added for debug prints
#include <stdexcept>
#include <unordered_set>
#include <cstdio>

namespace hgps {
DataSeries::DataSeries(std::size_t sample_size) : sample_size_{sample_size} {
    data_.emplace(core::Gender::male, std::map<std::string, std::vector<double>>{});
    data_.emplace(core::Gender::female, std::map<std::string, std::vector<double>>{});

    // Initialize income data structure for ALL possible income categories
    for (auto gender : {core::Gender::male, core::Gender::female}) {
        income_data_.emplace(gender,
                             std::map<core::Income, std::map<std::string, std::vector<double>>>{});
        // Initialize ALL possible income categories to ensure any enum value can be accessed
        for (auto income :
             {core::Income::unknown, core::Income::low, core::Income::middle, core::Income::high}) {
            income_data_[gender].emplace(income, std::map<std::string, std::vector<double>>{});
        }
    }
}

std::vector<double> &DataSeries::operator()(core::Gender gender, const std::string &key) {
    auto &vec = data_.at(gender).at(key);
    if (vec.size() != sample_size_) {
        std::fprintf(
            stderr,
            "[ERROR] DataSeries::operator(): vector size %zu != sample_size_ %zu for key '%s'\n",
            vec.size(), sample_size_, key.c_str());
        std::fflush(stderr);
    }
    return vec;
}

std::vector<double> &DataSeries::at(core::Gender gender, const std::string &key) {
    auto &vec = data_.at(gender).at(key);
    if (vec.size() != sample_size_) {
        std::fprintf(stderr,
                     "[ERROR] DataSeries::at(): vector size %zu != sample_size_ %zu for key '%s'\n",
                     vec.size(), sample_size_, key.c_str());
        std::fflush(stderr);
    }
    return vec;
}

const std::vector<double> &DataSeries::at(core::Gender gender, const std::string &key) const {
    const auto &vec = data_.at(gender).at(key);
    if (vec.size() != sample_size_) {
        std::fprintf(
            stderr,
            "[ERROR] DataSeries::at() const: vector size %zu != sample_size_ %zu for key '%s'\n",
            vec.size(), sample_size_, key.c_str());
        std::fflush(stderr);
    }
    return vec;
}

std::vector<double> &DataSeries::at(core::Gender gender, core::Income income,
                                    const std::string &key) {
    // Check if gender exists
    auto gender_it = income_data_.find(gender);
    if (gender_it == income_data_.end()) {
        throw std::out_of_range("Gender not found in income_data_");
    }

    // Check if income exists for this gender
    auto income_it = gender_it->second.find(income);
    if (income_it == gender_it->second.end()) {
        throw std::out_of_range("Income not found in income_data_");
    }

    // Check if key exists for this gender/income combination
    auto key_it = income_it->second.find(key);
    if (key_it == income_it->second.end()) {
        throw std::out_of_range("Key not found in income_data_");
    }

    return key_it->second;
}

const std::vector<double> &DataSeries::at(core::Gender gender, core::Income income,
                                          const std::string &key) const {
    return income_data_.at(gender).at(income).at(key);
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

void DataSeries::add_income_channels(const std::vector<std::string> &keys) {
    // Pre-allocate vectors to avoid repeated allocations
    std::vector<double> empty_vector(sample_size_);

    // Only create channels for the three main income categories that are actually used
    const std::array<core::Income, 3> income_categories = {core::Income::low, core::Income::middle,
                                                           core::Income::high};

    for (size_t i = 0; i < keys.size(); i++) {
        const auto &key = keys[i];
        auto channel_key = core::to_lower(key);

        // Only add to income-based channels - don't add to regular channels here
        // Regular channels are handled by add_channels() method
        for (auto gender : {core::Gender::male, core::Gender::female}) {
            auto &gender_income_data = income_data_.at(gender);
            for (auto income : income_categories) {
                // Check if channel already exists before adding
                auto &income_channels = gender_income_data.at(income);
                if (income_channels.find(channel_key) == income_channels.end()) {
                    income_channels.emplace(channel_key, empty_vector);
                }
            }
        }
    }
}

void DataSeries::add_income_channels_for_categories(
    const std::vector<std::string> &keys, const std::vector<core::Income> &income_categories) {
    // Pre-allocate vectors to avoid repeated allocations
    std::vector<double> empty_vector(sample_size_);

    for (size_t i = 0; i < keys.size(); i++) {
        const auto &key = keys[i];
        auto channel_key = core::to_lower(key);

        // Only add to income-based channels for the specified income categories
        for (auto gender : {core::Gender::male, core::Gender::female}) {
            auto &gender_income_data = income_data_.at(gender);
            for (auto income : income_categories) {
                // Check if channel already exists before adding
                auto &income_channels = gender_income_data.at(income);
                if (income_channels.find(channel_key) == income_channels.end()) {
                    income_channels.emplace(channel_key, empty_vector);
                }
            }
        }
    }
}

double &DataSeries::safe_at(std::vector<double> &vec, std::size_t index, const char *context) {
    if (index >= vec.size()) {
        std::fprintf(stderr, "[CRASH LOCATION] DataSeries::safe_at - index %zu >= vec.size() %zu in context: %s\n",
                     index, vec.size(), context ? context : "unknown");
        std::fflush(stderr);
        throw std::out_of_range(fmt::format("Index {} >= vector size {} in {}", index, vec.size(), context ? context : "unknown"));
    }
    return vec[index];
}

std::size_t DataSeries::size() const noexcept { return channels_.size(); }

std::size_t DataSeries::sample_size() const noexcept { return sample_size_; }

std::vector<core::Income> DataSeries::get_available_income_categories() const {
    std::vector<core::Income> categories;
    std::unordered_set<core::Income> seen;

    // Check which income categories have data
    for (const auto &[gender, income_map] : income_data_) {
        for (const auto &[income, channel_map] : income_map) {
            if (!channel_map.empty() && !seen.contains(income)) {
                categories.push_back(income);
                seen.insert(income);
            }
        }
    }

    return categories;
}

bool DataSeries::has_income_category(core::Gender gender, core::Income income) const {
    auto gender_it = income_data_.find(gender);
    if (gender_it == income_data_.end()) {
        return false;
    }

    auto income_it = gender_it->second.find(income);
    return income_it != gender_it->second.end();
}

std::string DataSeries::income_category_to_string(core::Income income) const {
    switch (income) {
    case core::Income::unknown:
        return "Unknown";
    case core::Income::low:
        return "LowIncome";
    case core::Income::middle:
        return "MiddleIncome";
    case core::Income::high:
        return "HighIncome";
    default:
        return "Unknown";
    }
}
} // namespace hgps
