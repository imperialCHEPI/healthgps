#pragma once
#include <map>

#include "HealthGPS.Core/interval.h"

namespace hgps {

	struct Mortality {
		Mortality(float males, float females)
			: males{ males }, females{ females }
		{}

		const float males{};
		const float females{};

		const float total() const noexcept { return males + females; }
	};

	struct Birth {
		Birth(float number_of_births, float bisth_sex_ratio)
			: number{ number_of_births }, sex_ratio{ bisth_sex_ratio }
		{}

		const float number{};
		const float sex_ratio{};
	};

	class LifeTable
	{
	public:
		LifeTable() = delete;
		LifeTable(std::map<int, Birth>&& births,
				  std::map<int, std::map<int, Mortality>>&& deaths);

		const Birth& get_births_at(const int year) const;

		const std::map<int, Mortality>& get_mortalities_at(const int year) const;

		double get_total_deaths_at(const int year) const;

		const core::IntegerInterval& time_limits() const noexcept;

		const core::IntegerInterval& age_limits() const noexcept;

		bool empty() const noexcept;

	private:
		std::map<int, Birth> birth_table_;
		std::map<int, std::map<int, Mortality>> death_table_;

		// Data limits cache 
		core::IntegerInterval time_range_{};
		core::IntegerInterval age_range_{};
	};
}

