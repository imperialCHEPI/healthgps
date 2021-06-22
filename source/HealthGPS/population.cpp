#include <stdexcept>

#include "population.h"

namespace hgps {

	Population::Population(const core::Country country, const std::string identy_col,
		const int start_time, const float dt_percent, const std::string linkage_col, 
		const core::IntegerInterval range)
		: country_{country}, identity_column_{identy_col},
		start_time_{ start_time }, delta_percent_{ dt_percent },
		linkage_column_{linkage_col}, age_range_{ range } {

		// TODO: Create a columns name wrapper
		if (identity_column_.length() < 2 || !std::isalpha(identity_column_.front())) {
			throw std::invalid_argument(
				"Invalid column name: minimum length of two and start with alpha character.");
		}

		if (dt_percent < 0.0 || dt_percent > 1.0) {
			throw std::invalid_argument(
				"Invalid percentage value, must be between in range [0, 1].");
		}
	}

	core::Country Population::country() const noexcept {
		return country_;
	}

	std::string Population::identity_column() const noexcept {
		return identity_column_;
	}

	int Population::start_time() const noexcept	{
		return start_time_;
	}

	float Population::delta_percent() const noexcept {
		return delta_percent_;
	}

	std::string Population::linkage_column() const noexcept
	{
		return std::string();
	}

	core::IntegerInterval Population::age_range() const noexcept {
		return age_range_;
	}
}
