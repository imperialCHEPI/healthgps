#include "data_series.h"
#include "HealthGPS.Core/string_util.h"

#include "fmt/core.h"
#include <stdexcept>

namespace hgps {
DataSeries::DataSeries(std::size_t sample_size) : sample_size_{sample_size}, channels_{}, data_{} {
    data_.emplace(core::Gender::male, std::map<std::string, std::vector<double>>{});
    data_.emplace(core::Gender::female, std::map<std::string, std::vector<double>>{});
}

std::vector<double> &DataSeries::operator()(core::Gender gender, std::string key) {
    return data_.at(gender).at(key);
}

std::vector<double> &DataSeries::at(core::Gender gender, std::string key) {
    return data_.at(gender).at(key);
}

const std::vector<double> &DataSeries::at(core::Gender gender, std::string key) const {
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
    for (auto &item : keys) {
        add_channel(item);
    }
}

std::size_t DataSeries::size() const noexcept { return channels_.size(); }

std::size_t DataSeries::sample_size() const noexcept { return sample_size_; }
} // namespace hgps
