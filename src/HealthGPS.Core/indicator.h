#pragma once
#include <string>

namespace hgps::core {

	/// @brief Birth rate indicator for a country data structure
	struct BirthItem
	{
		/// @brief The reference time
		int at_time;

		/// @brief The total number of births
		float number;

		/// @brief Sex Ratio at Birth (males per 100 female births)
		float sex_ratio;
	};
}