#pragma once

#include <HealthGPS.Core/poco.h>
#include <HealthGPS.Core/interval.h>

namespace hgps {

	class Settings
	{
	public:
		Settings(const core::Country country,
			const float size_fraction, const std::string linkage_col,
			const core::IntegerInterval age_range);

		core::Country country() const noexcept;

		float size_fraction() const noexcept;

		std::string linkage_column() const noexcept;

		core::IntegerInterval age_range() const noexcept;

	private:
		core::Country country_;
		float size_fraction_{};
		std::string linkage_column_;
		core::IntegerInterval age_range_;
	};
}