#pragma once
#include "gender_value.h"
#include "HealthGPS.Core/interval.h"

#include <map>

namespace hgps {

	/// @brief Mortality by gender
	using Mortality = GenderValue<float>;

	/// @brief Define the birth data type
	struct Birth {

		/// @brief Initialises a new instance of the Birth structure
		/// @param number_of_births Total number of births
		/// @param birth_sex_ratio Sex ratio at birth
		Birth(float number_of_births, float birth_sex_ratio)
			: number{ number_of_births }, sex_ratio{ birth_sex_ratio }
		{}

		/// @brief The total number of births.
		float number{};

		/// @brief Sex ratio at birth (males per 100 female births)
		float sex_ratio{};
	};

	/// @brief Defines the population life table data type
	class LifeTable
	{
	public:
		LifeTable() = delete;
		/// @brief Initialises a new instance of the LifeTable structure
		/// @param births Number of births per year
		/// @param deaths Number of deaths per year and age
		LifeTable(std::map<int, Birth>&& births,
				  std::map<int, std::map<int, Mortality>>&& deaths);

		/// @brief Gets the Birth indicators at a given time
		/// @param time_year Time of births
		/// @return The Birth indicator
		const Birth& get_births_at(int time_year) const;

		/// @brief Gets the Mortality indicators at a given time
		/// @param time_year Time of mortality
		/// @return The age associate Mortality indicators
		const std::map<int, Mortality>& get_mortalities_at(int time_year) const;

		/// @brief Gets the total number of deaths at a given time
		/// @param time_year Time of deaths
		/// @return Total number of deaths
		double get_total_deaths_at(int time_year) const;

		/// @brief Determine whether the LifeTable data contains a given age 
		/// @param age The age to check
		/// @return true, if the LifeTable data contains the age; otherwise, false.
		bool contains_age(int age) const noexcept;

		/// @brief Determine whether the LifeTable data contains a given time 
		/// @param time_year The time to check
		/// @return true, if the LifeTable data contains the time; otherwise, false.
		bool contains_time(int time_year) const noexcept;

		/// @brief Gets the LifeTable data time limits
		/// @return Data time limits
		const core::IntegerInterval& time_limits() const noexcept;

		/// @brief Gets the LifeTable data age limits
		/// @return Data age limits
		const core::IntegerInterval& age_limits() const noexcept;

		/// @brief Determine whether the LifeTable data is empty
		/// @return true, if the LifeTable data is empty; otherwise, false.
		bool empty() const noexcept;

	private:
		std::map<int, Birth> birth_table_;
		std::map<int, std::map<int, Mortality>> death_table_;

		// Data limits cache 
		core::IntegerInterval time_range_{};
		core::IntegerInterval age_range_{};
	};
}

