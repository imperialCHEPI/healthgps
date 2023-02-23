#pragma once

#include <tuple>

namespace hgps {
	namespace core {

		/// @brief Population mortality trends for a country data structure
		struct MortalityItem
		{
			/// @brief The country unique identifier code
			int location_id{};

			/// @brief Item reference time
			int at_time{};

			/// @brief Item reference time
			int with_age{};

			/// @brief Number of male deaths
			float males{};

			/// @brief Number of female deaths
			float females{};

			/// @brief Total number of deaths
			float total{};
		};

		inline bool operator< (MortalityItem const& lhs, MortalityItem const& rhs) {
			return std::tie(lhs.at_time, lhs.with_age) < std::tie(rhs.at_time, rhs.with_age);
		}

		inline bool operator> (MortalityItem const& lhs, MortalityItem const& rhs) {
			return std::tie(lhs.at_time, lhs.with_age) > std::tie(rhs.at_time, rhs.with_age);
		}
	}
}
