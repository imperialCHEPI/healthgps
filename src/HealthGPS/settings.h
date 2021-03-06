#pragma once

#include <HealthGPS.Core/poco.h>
#include <HealthGPS.Core/interval.h>

namespace hgps {

	class Settings
	{
	public:
		Settings(const core::Country& country, const float size_fraction,
			const core::IntegerInterval& age_range);

		const core::Country& country() const noexcept;

		const float& size_fraction() const noexcept;

		const core::IntegerInterval& age_range() const noexcept;

	private:
		core::Country country_;
		float size_fraction_{};
		core::IntegerInterval age_range_;
	};
}