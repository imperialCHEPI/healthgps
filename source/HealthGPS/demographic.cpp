#include <numeric>
#include <cassert>
#include "demographic.h"
#include "converter.h"

namespace hgps {
	DemographicModule::DemographicModule(
		std::map<int, std::map<int, PopulationRecord>>&& pop_data, LifeTable&& life_table)
		: pop_data_{ pop_data }, life_table_{ life_table }, birth_rates_{}, residual_death_rates_{} {
		if (pop_data_.empty()) {
			if (!life_table_.empty()) {
				throw std::invalid_argument("empty population and life table content mismatch.");
			}

			return;
		}
		else if (life_table_.empty()) {
			throw std::invalid_argument("population and empty life table content mismatch.");
		}

		auto first_entry = pop_data_.begin();
		auto time_range = core::IntegerInterval(first_entry->first, pop_data_.rbegin()->first);
		core::IntegerInterval age_range{};
		if (!first_entry->second.empty()) {
			age_range = core::IntegerInterval(
				first_entry->second.begin()->first,
				first_entry->second.rbegin()->first);
		}

		if (time_range != life_table.time_limits()) {
			throw std::invalid_argument("Population and life table time limits mismatch.");
		}

		if (age_range != life_table.age_limits()) {
			throw std::invalid_argument("Population and life table age limits mismatch.");
		}

		initialise_birth_rates();
	}

	SimulationModuleType DemographicModule::type() const noexcept {
		return SimulationModuleType::Demographic;
	}

	std::string DemographicModule::name() const noexcept {
		return "Demographic";
	}

	size_t DemographicModule::get_total_population(const int time_year) const noexcept {
		auto total = 0.0f;
		if (pop_data_.contains(time_year)) {
			auto year_data = pop_data_.at(time_year);
			total = std::accumulate(year_data.begin(), year_data.end(), 0.0f,
				[](const float previous, const auto& element)
				{ return previous + element.second.total(); });
		}

		return (size_t)total;
	}

	std::map<int, GenderPair> DemographicModule::get_age_gender_distribution(const int time_year) const noexcept {
		std::map<int, GenderPair> result;
		if (!pop_data_.contains(time_year)) {
			return result;
		}

		auto year_data = pop_data_.at(time_year);
		if (!year_data.empty()) {
			double total_ratio = 1.0 / get_total_population(time_year);

			for (auto& age : year_data) {
				result.emplace(age.first, GenderPair(
					age.second.males * total_ratio,
					age.second.females * total_ratio));
			}
		}

		return result;
	}

	GenderPair DemographicModule::get_birth_rate(const int time_year) const noexcept {
		if (birth_rates_.contains(time_year)) {
			return GenderPair{ birth_rates_(time_year, core::Gender::male),
							   birth_rates_(time_year, core::Gender::female) };
		}

		return GenderPair{ 0.0, 0.0 };
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

	void DemographicModule::update_residual_mortality(RuntimeContext& context, const DiseaseHostModule& disease_host) {
		auto excess_mortality_product = create_integer_gender_table<double>(life_table_.age_limits());
		auto excess_mortality_count = create_integer_gender_table<int>(life_table_.age_limits());
		for (const auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			auto product = calculate_excess_mortality_product(entity, disease_host);
			excess_mortality_product.at(entity.age, entity.gender) += product;
			excess_mortality_count.at(entity.age, entity.gender)++;
		}

		auto death_rates = create_death_rates_table(context.time_now());
		residual_death_rates_ = create_integer_gender_table<double>(life_table_.age_limits());
		auto start_age = life_table_.age_limits().lower();
		auto end_age = life_table_.age_limits().upper();
		for (int age = start_age; age <= end_age; age++) {
			auto male_count = excess_mortality_count.at(age, core::Gender::male);
			auto female_count = excess_mortality_count.at(age, core::Gender::female);
			auto male_average_product = excess_mortality_product.at(age, core::Gender::male) / male_count;
			auto female_average_product = excess_mortality_product.at(age, core::Gender::female) / female_count;

			residual_death_rates_.at(age, core::Gender::male) =
				1.0 - (1.0 - death_rates.at(age, core::Gender::male) / male_average_product);

			residual_death_rates_.at(age, core::Gender::female) =
				1.0 - (1.0 - death_rates.at(age, core::Gender::female) / female_average_product);
		}
	}

	void DemographicModule::initialise_birth_rates() {
		birth_rates_ = create_integer_gender_table<double>(life_table_.time_limits());
		auto start_time = life_table_.time_limits().lower();
		auto end_time = life_table_.time_limits().upper();
		for (int year = start_time; year <= end_time; year++)
		{
			auto births = life_table_.get_births_at(year);
			auto population_size = get_total_population(year);

			double male_birth_rate = births.number * births.sex_ratio / (1.0f + births.sex_ratio) / population_size;
			double female_birth_rate = births.number / (1.0f + births.sex_ratio) / population_size;

			birth_rates_.at(year, core::Gender::male) = male_birth_rate;
			birth_rates_.at(year, core::Gender::female) = female_birth_rate;
		}
	}

	GenderTable<int, double> DemographicModule::create_death_rates_table(const int time_year) {
		auto population = pop_data_.at(time_year);
		auto mortality = life_table_.get_mortalities_at(time_year);
		auto death_rates = create_integer_gender_table<double>(life_table_.age_limits());
		auto start_age = life_table_.age_limits().lower();
		auto end_age = life_table_.age_limits().upper();
		for (int age = start_age; age <= end_age; age++) {
			death_rates.at(age, core::Gender::male) = mortality.at(age).males / population.at(age).males;
			death_rates.at(age, core::Gender::female) = mortality.at(age).females / population.at(age).females;
		}

		return death_rates;
	}

	double DemographicModule::calculate_excess_mortality_product(
		const Person& entity, const DiseaseHostModule& disease_host) const {
		auto product = 1.0;
		auto excessMortality = 0.0;
		for (const auto& item : entity.diseases) {
			if (item.second.status == DiseaseStatus::Active) {
				auto excessMortality = disease_host.get_excess_mortality(item.first, entity.age, entity.gender);
				product *= 1.0 - excessMortality;
			}
		}

		return product;
	}

	std::unique_ptr<DemographicModule> build_demographic_module(core::Datastore& manager, ModelInput& config) {
		// year => age [age, male, female]
		auto pop_data = std::map<int, std::map<int, PopulationRecord>>();

		auto min_time = std::min(config.start_time(), config.settings().reference_time());
		auto max_time = config.stop_time();
		auto time_filter = [&min_time, &max_time](const unsigned int& value) {
			return value >= min_time && value <= max_time;
		};

		auto pop = manager.get_population(config.settings().country(), time_filter);
		for (auto& item : pop) {
			pop_data[item.year].emplace(item.age, PopulationRecord(item.age, item.males, item.females));
		}

		auto births = manager.get_birth_indicators(config.settings().country(), time_filter);
		auto deaths = manager.get_mortality(config.settings().country(), time_filter);
		auto life_table = detail::StoreConverter::to_life_table(births, deaths);

		return std::make_unique<DemographicModule>(std::move(pop_data), std::move(life_table));
	}
}