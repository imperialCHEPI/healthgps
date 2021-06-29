#pragma once

#include <HealthGPS.Core/poco.h>
#include <HealthGPS.Core/interval.h>

namespace hgps {

	class Settings
	{
	public:
		Settings(const core::Country country, const std::string identy_col, 
			const int start_time, const float dt_percent,
			const std::string linkage_col, const core::IntegerInterval age_range);

		core::Country country() const noexcept;

		std::string identity_column() const noexcept;

		int start_time() const noexcept;

		float delta_percent() const noexcept;

		std::string linkage_column() const noexcept;

		core::IntegerInterval age_range() const noexcept;

	private:
		core::Country country_;
		std::string identity_column_;
		int start_time_{};
		float delta_percent_{};
		std::string linkage_column_;
		core::IntegerInterval age_range_;
	};
}