#include "life_table.h"
#include <numeric>
#include <fmt/format.h>

namespace hgps {
	LifeTable::LifeTable(std::map<int, Birth>&& births,
		std::map<int, std::map<int, Mortality>>&& deaths)
		: birth_table_{ std::move(births) }, death_table_{ std::move(deaths) }
		, time_range_{}, age_range_{}
	{
		if (birth_table_.empty()) {
			if (!death_table_.empty()) {
				throw std::invalid_argument("empty births and deaths content mismatch");
			}

			return;
		}
		else if (death_table_.empty()) {
			throw std::invalid_argument("births and empty deaths content mismatch");
		}

		if (birth_table_.begin()->first != death_table_.begin()->first ||
			birth_table_.rbegin()->first != death_table_.rbegin()->first) {
			throw std::invalid_argument("births and deaths time range mismatch.");
		}

		auto time_entry = death_table_.begin();
		time_range_ = core::IntegerInterval(time_entry->first, death_table_.rbegin()->first);
		if (!time_entry->second.empty()) {
			age_range_ = core::IntegerInterval(
				time_entry->second.begin()->first,
				time_entry->second.rbegin()->first);
		}
	}

	const Birth& LifeTable::get_births_at(const int time_year) const	{
		if (!birth_table_.contains(time_year)) {
			throw std::out_of_range(fmt::format("The year must not be outside of the data limits."));
		}

		return birth_table_.at(time_year);
	}

	const std::map<int, Mortality>& LifeTable::get_mortalities_at(const int time_year) const {
		if (!death_table_.contains(time_year)) {
			throw std::out_of_range(fmt::format("The year must not be outside of the data limits."));
		}

		return death_table_.at(time_year);
	}

	double LifeTable::get_total_deaths_at(const int time_year) const {
		if (!death_table_.contains(time_year)) {
			throw std::out_of_range(fmt::format("The year must not be outside of the data limits."));
		}

		auto year_data = get_mortalities_at(time_year);
		auto total = std::accumulate(year_data.cbegin(), year_data.cend(), 0.0f,
			[](const float previous, const auto& element) { return previous + element.second.total(); });

		return total;
	}

	bool LifeTable::contains_age(const int age) const noexcept {
		return age_range_.lower() <= age && age <= age_range_.upper();
	}

	bool LifeTable::contains_time(const int time_year) const noexcept {
		return death_table_.contains(time_year);
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