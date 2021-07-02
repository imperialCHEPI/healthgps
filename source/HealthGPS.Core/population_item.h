#pragma once
#include <tuple>

namespace hgps {
	namespace core {

		struct PopulationItem
		{
			int code{};
			int year{};
			int age{};
			float males{};
			float females{};
			float total{};
		};

		inline bool operator< (PopulationItem const& lhs, PopulationItem const& rhs) {
			return std::tie(lhs.year, lhs.year) < std::tie(rhs.age, rhs.age);
		}

		inline bool operator> (PopulationItem const& lhs, PopulationItem const& rhs) {
			return std::tie(lhs.year, lhs.year) > std::tie(rhs.age, rhs.age);
		}
	}
}
