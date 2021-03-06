#include "data_series.h"
#include "HealthGPS.Core/string_util.h"

#include "fmt/core.h"
#include <stdexcept>

namespace hgps
{
	DataSeries::DataSeries(const unsigned int sample_size)
		: sample_size_{ sample_size }
		, sample_id_channel_(sample_size)
		, channels_{}, data_{}
	{
		data_.emplace(core::Gender::male, std::map<std::string, std::vector<double>>{});
		data_.emplace(core::Gender::female, std::map<std::string, std::vector<double>>{});

		for (auto id = 0u; id < sample_size; id++) {
			sample_id_channel_.at(id) = id;
		}
	}

	std::vector<double>& DataSeries::operator()(core::Gender gender, std::string key) {
		return data_.at(gender).at(key);
	}

	std::vector<double>& DataSeries::at(core::Gender gender, std::string key) {
		return data_.at(gender).at(key);
	}

	const std::vector<double>& DataSeries::at(core::Gender gender, std::string key) const {
		return data_.at(gender).at(key);
	}

	const std::vector<std::string>& DataSeries::channels() const noexcept {
		return channels_;
	}

	const std::vector<int>& DataSeries::index_channel() const noexcept {
		return  sample_id_channel_;
	}

	void DataSeries::add_channel(std::string key) {
		auto channel_key = core::to_lower(key);
		if (core::case_insensitive::contains(channels_, channel_key)) {
			throw std::logic_error(fmt::format("No duplicated channel key: {} is not allowed.", key));
		}

		channels_.emplace_back(channel_key);
		data_.at(core::Gender::male).emplace(channel_key, std::vector<double>(sample_size_));
		data_.at(core::Gender::female).emplace(channel_key, std::vector<double>(sample_size_));
	}

	void DataSeries::add_channels(const std::vector<std::string>& keys) {
		for (auto& item : keys) {
			add_channel(item);
		}
	}

	std::size_t DataSeries::size() const noexcept {
		return channels_.size();
	}
}
