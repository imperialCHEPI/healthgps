#pragma once

#include <tuple>

namespace hgps {
	namespace core {

		struct MortalityItem
		{
			int location_id{};
			int year{};
			int age{};
			float males{};
			float females{};
			float total{};
		};

		inline bool operator< (MortalityItem const& lhs, MortalityItem const& rhs) {
			return std::tie(lhs.year, lhs.age) < std::tie(rhs.year, rhs.age);
		}

		inline bool operator> (MortalityItem const& lhs, MortalityItem const& rhs) {
			return std::tie(lhs.year, lhs.age) > std::tie(rhs.year, rhs.age);
		}
	}
}
