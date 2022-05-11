#pragma once
#include "HealthGPS.Core/forward_type.h"

#include <string>
#include <vector>
#include <map>

namespace hgps {

	class DataSeries
	{
	public:
		DataSeries() = delete;
		DataSeries(const unsigned int max_age);

		std::vector<double>& operator()(core::Gender gender, std::string key);

		std::vector<double>& at(core::Gender gender, std::string key);

		const std::vector<double>& at(core::Gender gender, std::string key) const;

		const std::vector<std::string>& channels() const noexcept;

		const std::vector<int>& index_channel() const noexcept;

		void add_channel(std::string key);

		void add_channels(const std::vector<std::string>& keys);

		std::size_t size() const noexcept;

	private:
		unsigned int sample_size_;
		std::vector<int> sample_id_channel_;
		std::vector<std::string> channels_;
		std::map<core::Gender, std::map<std::string, std::vector<double>>> data_;
	};
}
