#pragma once

#include <HealthGPS.Core/poco.h>

namespace hgps {

	class Population
	{
	public:
		Population(core::Country country, std::string identy_column,
			const int start_time, double dt_percent);

		core::Country country() const noexcept;

		std::string identity_column() const noexcept;

		int start_time() const noexcept;

		double delta_percent() const noexcept;

	private:
		const core::Country country_;
		const std::string identity_column_;
		const int start_time_{};
		const double delta_percent_{};
	};
}