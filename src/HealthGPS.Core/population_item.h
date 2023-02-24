#pragma once
#include <tuple>

namespace hgps::core {

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

	/// @brief Determine whether a specified PopulationItem is less than another instance.
	/// @param lhs The first instance to compare.
	/// @param rhs The second instance to compare.
	/// @return true if left instance is less than right instance; otherwise, false.
	inline bool operator< (PopulationItem const& lhs, PopulationItem const& rhs) {
		return std::tie(lhs.at_time, lhs.with_age) < std::tie(rhs.at_time, rhs.with_age);
	}

	/// @brief Determine whether a specified PopulationItem is greater than another instance.
	/// @param lhs The first instance to compare.
	/// @param rhs The second instance to compare.
	/// @return true if left instance is greater than right instance; otherwise, false.
	inline bool operator> (PopulationItem const& lhs, PopulationItem const& rhs) {
		return std::tie(lhs.at_time, lhs.with_age) > std::tie(rhs.at_time, rhs.with_age);
	}
}
