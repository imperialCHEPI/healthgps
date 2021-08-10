#include "life_table.h"

namespace hgps {
	LifeTable::LifeTable(std::map<int, Birth>&& births,
		std::map<int, std::map<int, Mortality>>&& deaths)
		: birth_table_{ births }, death_table_{ deaths }, time_range_{}, age_range_{}
	{
		if (births.empty()) {
			if (!deaths.empty()) {
				throw std::invalid_argument("empty births and deaths content mismatch.");
			}

			return;
		}
		else if (deaths.empty()) {
			throw std::invalid_argument("births and empty deaths content mismatch.");
		}

		if (births.begin()->first != deaths.begin()->first ||
			births.rbegin()->first != deaths.rbegin()->first) {
			throw std::invalid_argument("births and deaths time range mismatch.");
		}

		auto time_entry = deaths.begin();
		time_range_ = core::IntegerInterval(time_entry->first, deaths.rbegin()->first);
		if (!time_entry->second.empty()) {
			age_range_ = core::IntegerInterval(
				time_entry->second.begin()->first,
				time_entry->second.rbegin()->first);
		}
	}

	const Birth& LifeTable::get_births_at(const int year) const	{
		if (!birth_table_.contains(year)) {
			throw std::out_of_range(std::format("The year must not be outside of the data limits."));
		}

		return birth_table_.at(year);
	}

	const std::map<int, Mortality>& LifeTable::get_mortalities_at(const int year) const {
		if (!death_table_.contains(year)) {
			throw std::out_of_range(std::format("The year must not be outside of the data limits."));
		}

		return death_table_.at(year);
	}

	double LifeTable::get_total_deaths_at(const int year) const {
		if (!death_table_.contains(year)) {
			throw std::out_of_range(std::format("The year must not be outside of the data limits."));
		}

		auto result_sum = 0.0;
		for (auto& value : get_mortalities_at(year)) {
			result_sum += value.second.total();
		}

		return result_sum;
	}

	const core::IntegerInterval& LifeTable::time_limits() const noexcept {
		return time_range_;
	}

	const core::IntegerInterval& LifeTable::age_limits() const noexcept	{
		return age_range_;
	}

	bool LifeTable::empty() const noexcept {
		return birth_table_.empty() || death_table_.empty();
	}
}