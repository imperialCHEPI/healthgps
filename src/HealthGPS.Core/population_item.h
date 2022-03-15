#pragma once
#include <tuple>

namespace hgps {
	namespace core {

		struct PopulationItem
		{
			int location_id{};
			int year{};
			int age{};
			float males{};
			float females{};
			float total{};
		};

		inline bool operator< (PopulationItem const& lhs, PopulationItem const& rhs) {
			return std::tie(lhs.year, lhs.age) < std::tie(rhs.year, rhs.age);
		}

		inline bool operator> (PopulationItem const& lhs, PopulationItem const& rhs) {
			return std::tie(lhs.year, lhs.age) > std::tie(rhs.year, rhs.age);
		}
	}
}
