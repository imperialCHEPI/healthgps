#pragma once
#include <string>

namespace hgps::core {

	/// @brief Country ISO-3166 definition data structure
	struct Country
	{
		/// @brief Unique code identifier, e.g. 250
		int code{};

		/// @brief Full country name, e.g. France
		std::string name{};

		/// @brief The alpha 2 characters unique identifier, e.g., FR
		std::string alpha2{};

		/// @brief The alpha 3 characters unique identifier, e.g., FRA
		std::string alpha3{};
	};

	/// @brief Greater-than operation for country data type
	/// @param lhs The left country to compare
	/// @param rhs The right country to compare
	/// @return true if the left country is greater than the left country, otherwise false
	inline bool operator> (const Country& lhs, const Country& rhs) { return lhs.name > rhs.name; }

	/// @brief Less-than operation for country data type
	/// @param lhs The left country to compare
	/// @param rhs The right country to compare
	/// @return true if the left country is less than the left country, otherwise false
	inline bool operator< (const Country& lhs, const Country& rhs) { return lhs.name < rhs.name; }
}
