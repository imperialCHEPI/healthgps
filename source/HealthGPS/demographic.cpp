#include <numeric>
#include <cassert>
#include "demographic.h"
#include "converter.h"
#include "baseline_sync_message.h"

namespace hgps {
	PopulationModule::PopulationModule(
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

	SimulationModuleType PopulationModule::type() const noexcept {
		return SimulationModuleType::Demographic;
	}

	std::string PopulationModule::name() const noexcept {
		return "Demographic";
	}

	std::size_t PopulationModule::get_total_population_size(const int time_year) const noexcept {
		auto total = 0.0f;
		if (pop_data_.contains(time_year)) {
			auto year_data = pop_data_.at(time_year);
			total = std::accumulate(year_data.begin(), year_data.end(), 0.0f,
				[](const float previous, const auto& element)
				{ return previous + element.second.total(); });
		}

		return static_cast<std::size_t>(total);
	}

	double PopulationModule::get_total_deaths(const int time_year) const noexcept {
		if (life_table_.contains_time(time_year)) {
			return life_table_.get_total_deaths_at(time_year);
		}

		return 0.0;
	}

	const std::map<int, PopulationRecord>& PopulationModule::get_population_distribution(const int time_year) const {
		return pop_data_.at(time_year);
	}

	std::map<int, DoubleGenderValue> PopulationModule::get_age_gender_distribution(const int time_year) const noexcept {
		std::map<int, DoubleGenderValue> result;
		if (!pop_data_.contains(time_year)) {
			return result;
		}

		auto year_data = pop_data_.at(time_year);
		if (!year_data.empty()) {
			double total_ratio = 1.0 / get_total_population_size(time_year);

			for (auto& age : year_data) {
				result.emplace(age.first, DoubleGenderValue(
					age.second.males * total_ratio,
					age.second.females * total_ratio));
			}
		}

		return result;
	}

	DoubleGenderValue PopulationModule::get_birth_rate(const int time_year) const noexcept {
		if (birth_rates_.contains(time_year)) {
			return DoubleGenderValue{ birth_rates_(time_year, core::Gender::male),
							   birth_rates_(time_year, core::Gender::female) };
		}

		return DoubleGenderValue{ 0.0, 0.0 };
	}

	double PopulationModule::get_residual_death_rate(const int& age, core::Gender& gender) const noexcept {
		if (residual_death_rates_.contains(age)) {
			return residual_death_rates_.at(age, gender);
		}

		return 0.0;
	}

	void PopulationModule::initialise_population(RuntimeContext& context) {
		auto age_gender_dist = get_age_gender_distribution(context.start_time());
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

	void PopulationModule::update_population(RuntimeContext& context, const DiseaseHostModule& disease_host) {
		auto initial_pop_size = context.population().current_active_size();
		auto expected_pop_size = get_total_population_size(context.time_now());
		auto expected_num_deaths = get_total_deaths(context.time_now());

		// apply death events and update basic information (age)
		auto number_of_deaths = update_age_and_death_events(context, disease_host);

		// apply births events
		auto last_year_births_rate = get_birth_rate(context.time_now() - 1);
		auto number_of_boys = static_cast<int> (last_year_births_rate.male * initial_pop_size);
		auto number_of_girls = static_cast<int>(last_year_births_rate.female * initial_pop_size);
		context.population().add_newborn_babies(number_of_boys, core::Gender::male);
		context.population().add_newborn_babies(number_of_girls, core::Gender::female);

		// Calculate statistics.
		auto number_of_births = number_of_boys + number_of_girls;
		auto expected_migration = (expected_pop_size / 100.0) - initial_pop_size - number_of_births + number_of_deaths;
		auto middle_pop_size = context.population().current_active_size();
		auto pop_size_diff = (expected_pop_size / 100.0) - middle_pop_size;

		auto simulated_death_rate = number_of_deaths * 1000.0 / initial_pop_size;
		auto expected_death_rate = expected_num_deaths * 1000.0 / expected_pop_size;
		auto percent_difference = 100 * (simulated_death_rate / expected_death_rate - 1);
		context.metrics()["SimulatedDeathRate"] = simulated_death_rate;
		context.metrics()["ExpectedDeathRate"] = expected_death_rate;
		context.metrics()["DeathRateDeltaPercent"] = percent_difference;
	}

	void PopulationModule::update_residual_mortality(RuntimeContext& context, const DiseaseHostModule& disease_host) {
		if (context.scenario().type() == ScenarioType::baseline) {
			auto residual_mortality = calculate_residual_mortality(context, disease_host);
			residual_death_rates_ = residual_mortality;

			context.scenario().channel().send(std::make_unique<ResidualMortalityMessage>(
				context.current_run(), context.time_now(), std::move(residual_mortality)));
		}
		else {
			auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
			if (message.has_value()) {
				auto& basePtr = message.value();
				auto messagePrt = dynamic_cast<ResidualMortalityMessage*>(basePtr.get());
				if (messagePrt) {
					residual_death_rates_ = messagePrt->data();
				}
				else {
					throw std::runtime_error(
						"Simulation out of sync, failed to receive a residual mortality message");
				}
			}
			else {
				throw std::runtime_error(
					"Simulation out of sync, receive residual mortality message has timed out");
			}
		}
	}

	void PopulationModule::initialise_birth_rates() {
		birth_rates_ = create_integer_gender_table<double>(life_table_.time_limits());
		auto start_time = life_table_.time_limits().lower();
		auto end_time = life_table_.time_limits().upper();
		for (int year = start_time; year <= end_time; year++)
		{
			auto births = life_table_.get_births_at(year);
			auto population_size = get_total_population_size(year);

			double male_birth_rate = births.number * births.sex_ratio / (1.0f + births.sex_ratio) / population_size;
			double female_birth_rate = births.number / (1.0f + births.sex_ratio) / population_size;

			birth_rates_.at(year, core::Gender::male) = male_birth_rate;
			birth_rates_.at(year, core::Gender::female) = female_birth_rate;
		}
	}

	GenderTable<int, double> PopulationModule::create_death_rates_table(const int time_year) {
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

	GenderTable<int, double> PopulationModule::calculate_residual_mortality(
		RuntimeContext& context, const DiseaseHostModule& disease_host) {
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
		auto residual_mortality = create_integer_gender_table<double>(life_table_.age_limits());
		auto start_age = life_table_.age_limits().lower();
		auto end_age = life_table_.age_limits().upper();
		for (int age = start_age; age <= end_age; age++) {
			auto male_count = excess_mortality_count.at(age, core::Gender::male);
			auto female_count = excess_mortality_count.at(age, core::Gender::female);
			auto male_average_product = excess_mortality_product.at(age, core::Gender::male) / male_count;
			auto female_average_product = excess_mortality_product.at(age, core::Gender::female) / female_count;

			residual_mortality.at(age, core::Gender::male) =
				1.0 - (1.0 - death_rates.at(age, core::Gender::male) / male_average_product);

			residual_mortality.at(age, core::Gender::female) =
				1.0 - (1.0 - death_rates.at(age, core::Gender::female) / female_average_product);
		}

		return residual_mortality;
	}

	double PopulationModule::calculate_excess_mortality_product(
		const Person& entity, const DiseaseHostModule& disease_host) const {
		auto product = 1.0;
		for (const auto& item : entity.diseases) {
			if (item.second.status == DiseaseStatus::active) {
				auto excess_mortality = disease_host.get_excess_mortality(item.first, entity);
				product *= 1.0 - excess_mortality;
			}
		}

		return product;
	}

	int PopulationModule::update_age_and_death_events(RuntimeContext& context, const DiseaseHostModule& disease_host) {
		auto max_age = static_cast<unsigned int>(context.age_range().upper());
		auto number_of_deaths = 0;
		for (auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			if (entity.age >= max_age) {
				entity.is_alive = false;
				entity.time_of_death = context.time_now();
				number_of_deaths++;
			}
			else {

				// calculate death probability based on the health status
				auto death_rate = get_residual_death_rate(entity.age, entity.gender);
				auto product = 1.0 - death_rate;
				for (const auto& item : entity.diseases) {
					if (item.second.status == DiseaseStatus::active) {
						auto excess_mortality = disease_host.get_excess_mortality(item.first, entity);
						product *= 1.0 - excess_mortality;
					}
				}

				auto death_probability = 1.0 - product;
				auto hazard = context.random().next_double();
				if (hazard < death_probability) {
					entity.is_alive = false;
					entity.time_of_death = context.time_now();
					number_of_deaths++;
				}
			}

			// Update basic information
			if (entity.is_active()) {
				entity.age = entity.age + 1;
				if (entity.age >= max_age) {
					entity.is_alive = false;
					entity.time_of_death = context.time_now();
				}
			}
		}

		return number_of_deaths;
	}

	std::unique_ptr<PopulationModule> build_population_module(Repository& repository, const ModelInput& config) {
		// year => age [age, male, female]
		auto pop_data = std::map<int, std::map<int, PopulationRecord>>();

		auto min_time = config.start_time();
		auto max_time = config.stop_time();
		auto time_filter = [&min_time, &max_time](const unsigned int& value) {
			return value >= min_time && value <= max_time;
		};

		auto pop = repository.manager().get_population(config.settings().country(), time_filter);
		for (auto& item : pop) {
			pop_data[item.year].emplace(item.age, PopulationRecord(item.age, item.males, item.females));
		}

		auto births = repository.manager().get_birth_indicators(config.settings().country(), time_filter);
		auto deaths = repository.manager().get_mortality(config.settings().country(), time_filter);
		auto life_table = detail::StoreConverter::to_life_table(births, deaths);

		return std::make_unique<PopulationModule>(std::move(pop_data), std::move(life_table));
	}
}