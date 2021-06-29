#include "demographic.h"
#include <numeric>

namespace hgps {
	Demographic::Demographic(std::map<int, std::map<int, AgeRecord>>&& data)
		: data_{ data } {
		if (!data_.empty()) {
			auto first_entry = data_.begin();
			time_range_ = core::IntegerInterval(first_entry->first, (--data_.end())->first);
			if (!first_entry->second.empty()) {
				age_range_ = core::IntegerInterval(
					first_entry->second.begin()->first,
					(--first_entry->second.end())->first);
			}
		}
	}

	SimulationModuleType Demographic::type() const {
		return SimulationModuleType::Demographic;
	}

	std::string Demographic::name() const {
		return "Demographic";
	}

	size_t Demographic::get_total_population(int time_year) const noexcept {
		auto total = 0.0f;
		if (data_.contains(time_year)) {
			auto year_data = data_.at(time_year);
			total = std::accumulate(year_data.begin(), year_data.end(), 0.0f,
				[](const float previous, const auto& element)
				{ return previous + element.second.total(); });
		}

		return (size_t)total;
	}

	std::map<int, GenderPair> Demographic::get_age_gender_distribution(int time_year) const noexcept {

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

	void Demographic::execute(std::string_view command,
		RandomBitGenerator& generator, std::vector<Entity>& entities) {
	}

	void Demographic::execute(std::string_view command,
		RandomBitGenerator& generator, Entity& entity) {
	}

	std::unique_ptr<Demographic> build_demographic_module(core::Datastore& manager, ModelInput& config) {
		// year => age [age, male, female]
		auto data = std::map<int, std::map<int, AgeRecord>>();

		auto pop = manager.get_population(config.settings().country(), [&config](const unsigned int& value) { 
			return value >= config.start_time() && value <= config.stop_time(); });

		for (auto& item : pop) {
			data[item.year].emplace(item.age, AgeRecord(item.age, item.males, item.females));
		}

		return std::make_unique<Demographic>(std::move(data));
	}
}