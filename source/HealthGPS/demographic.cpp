#include "demographic.h"
#include <numeric>
#include <cassert>

namespace hgps {
	DemographicModule::DemographicModule(std::map<int, std::map<int, PopulationRecord>>&& data)
		: data_{ data } {
		if (!data_.empty()) {
			auto first_entry = data_.begin();
			age_range_ = core::IntegerInterval(first_entry->first, data_.rbegin()->first);
			if (!first_entry->second.empty()) {
				age_range_ = core::IntegerInterval(
					first_entry->second.begin()->first,
					first_entry->second.rbegin()->first);
			}
		}
	}

	SimulationModuleType DemographicModule::type() const noexcept {
		return SimulationModuleType::Demographic;
	}

	std::string DemographicModule::name() const noexcept {
		return "Demographic";
	}

	size_t DemographicModule::get_total_population(const int time_year) const noexcept {
		auto total = 0.0f;
		if (data_.contains(time_year)) {
			auto year_data = data_.at(time_year);
			total = std::accumulate(year_data.begin(), year_data.end(), 0.0f,
				[](const float previous, const auto& element)
				{ return previous + element.second.total(); });
		}

		return (size_t)total;
	}

	std::map<int, GenderPair> DemographicModule::get_age_gender_distribution(const int time_year) const noexcept {
		std::map<int, GenderPair> result;
		if (!data_.contains(time_year)) {
			return result;
		}

		auto year_data = data_.at(time_year);
		if (!year_data.empty()) {
			double total_ratio = 1.0 / get_total_population(time_year);

			for (auto& age : year_data) {
				result.emplace(age.first, GenderPair(
					age.second.num_males * total_ratio,
					age.second.num_females * total_ratio));
			}
		}

		return result;
	}

	void DemographicModule::initialise_population(RuntimeContext& context) {
		auto age_gender_dist = get_age_gender_distribution(context.reference_time());
		auto index = 0;
		auto pop_size = static_cast<int>(context.population().size());
		auto entry_count = 0;
		for (auto& entry : age_gender_dist) {
			entry_count++;
			auto num_males = static_cast<int>(std::round(pop_size * entry.second.male));
			auto num_females = static_cast<int>(std::round(pop_size * entry.second.female));
			auto num_required = index + num_males + num_females;
			auto pop_diff = pop_size - num_required;
			// Final adjustment due to rounding errors
			if (entry_count == age_gender_dist.size() && pop_diff != 0) {
				if (pop_diff > 0) {
					int half_diff = pop_diff / 2;
					num_males += half_diff;
					num_females += half_diff;
					if (entry.second.male > entry.second.female) {
						num_males += pop_diff - (half_diff * 2);
					}
					else {
						num_females += pop_diff - (half_diff * 2);
					}
				}
				else {
					pop_diff *= -1;
					if (entry.second.male > entry.second.female) {
						num_females -= pop_diff;
						if (num_females < 0) {
							num_males += num_females;
						}
					}
					else {
						num_males -= pop_diff;
						if (num_males < 0) {
							num_females += num_males;
						}
					}

					num_males = std::max(0, num_males);
					num_females = std::max(0, num_females);
				}

				pop_diff = pop_size - (index + num_males + num_females);
				assert(pop_diff == 0);
			}

			// [index, index + num_males)
			for (size_t i = 0; i < num_males; i++) {
				context.population()[index].age = entry.first;
				context.population()[index].gender = core::Gender::male;
				index++;
			}

			// [index + num_males, num_required)
			for (size_t i = 0; i < num_females; i++) {
				context.population()[index].age = entry.first;
				context.population()[index].gender = core::Gender::female;
				index++;
			}
		}

		assert(index == pop_size);
	}

	std::unique_ptr<DemographicModule> build_demographic_module(core::Datastore& manager, ModelInput& config) {
		// year => age [age, male, female]
		auto data = std::map<int, std::map<int, PopulationRecord>>();

		auto min_time = std::min(config.start_time(), config.settings().reference_time());

		auto pop = manager.get_population(config.settings().country(), [&](const unsigned int& value) {
			return value >= min_time && value <= config.stop_time(); });

		for (auto& item : pop) {
			data[item.year].emplace(item.age, PopulationRecord(item.age, item.males, item.females));
		}

		return std::make_unique<DemographicModule>(std::move(data));
	}
}