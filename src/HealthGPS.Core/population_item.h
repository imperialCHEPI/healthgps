#pragma once
#include <tuple>

namespace hgps {
	namespace core {

		/// @brief Population trends item data structure
		struct PopulationItem
		{
			/// @brief The country unique identifier code
			int location_id{};

			/// @brief Item reference time
			int at_time{};

			/// @brief Item reference time
			int with_age{};

			/// @brief Number of males
			float males{};

			/// @brief Number of females
			float females{};

			/// @brief Total number of males and female
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
