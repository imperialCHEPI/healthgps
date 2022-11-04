#pragma once
#include <tuple>

namespace hgps {
	namespace core {

		struct PopulationItem
		{
			int location_id{};
			int at_time{};
			int with_age{};
			float males{};
			float females{};
			float total{};
		};

		inline bool operator< (PopulationItem const& lhs, PopulationItem const& rhs) {
			return std::tie(lhs.at_time, lhs.with_age) < std::tie(rhs.at_time, rhs.with_age);
		}

		inline bool operator> (PopulationItem const& lhs, PopulationItem const& rhs) {
			return std::tie(lhs.at_time, lhs.with_age) > std::tie(rhs.at_time, rhs.with_age);
		}
	}
}
