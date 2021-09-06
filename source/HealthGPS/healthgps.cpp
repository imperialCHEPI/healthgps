#include <iostream>
#include <algorithm>
#include <numeric>

#include "healthgps.h"
#include "mtrandom.h"
#include "univariate_visitor.h"
#include "info_message.h"
#include "converter.h"

#include "hierarchical_model.h"

namespace hgps {
	HealthGPS::HealthGPS(SimulationModuleFactory& factory, ModelInput& config,
		EventAggregator& bus, RandomBitGenerator&& generator)
		: Simulation(config, std::move(generator)),
		context_{ bus, rnd_, config_.risk_mapping(), config_.diseases(), config_.settings().age_range() }
	{
		// Create required modules, should change to shared_ptr
		auto ses_base = factory.create(SimulationModuleType::SES, config_);
		auto dem_base = factory.create(SimulationModuleType::Demographic, config_);
		auto risk_base = factory.create(SimulationModuleType::RiskFactor, config_);
		auto disease_base = factory.create(SimulationModuleType::Disease, config_);
		auto analysis_base = factory.create(SimulationModuleType::Analysis, config_);

		ses_ = std::static_pointer_cast<SESModule>(ses_base);
		demographic_ = std::static_pointer_cast<DemographicModule>(dem_base);
		risk_factor_ = std::static_pointer_cast<RiskFactorModule>(risk_base);
		disease_ = std::static_pointer_cast<DiseaseHostModule>(disease_base);
		analysis_ = std::static_pointer_cast<AnalysisModule>(analysis_base);
	}

	void HealthGPS::initialize()
	{
		// Reset random number generator
		if (config_.seed().has_value()) {
			rnd_.seed(config_.seed().value());
		}

		end_time_ = adevs::Time(config_.stop_time(), 0);
		std::cout << "Microsimulation algorithm initialised." << std::endl;
	}

	void HealthGPS::terminate() {
		std::cout << "Microsimulation algorithm terminate." << std::endl;
	}

	void HealthGPS::set_current_run(const unsigned int run_number) noexcept {
		context_.set_current_run(run_number);
	}

	std::string HealthGPS::name() {
		// TODO: replace with scenario type
		return "baseline";
	}

	adevs::Time HealthGPS::init(adevs::SimEnv<int>* env)
	{
		auto start = std::chrono::steady_clock::now();
		auto world_time = config_.start_time();
		context_.set_current_time(world_time);
		initialise_population();
		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;

		auto message = std::format("[{:4},{}] population size: {}, elapsed: {}ms",
			env->now().real, env->now().logical, context_.population().initial_size(), elapsed.count());
		context_.publish(std::make_unique<InfoEventMessage>(
			name(), ModelAction::start, context_.current_run(), context_.time_now(), message));

		return env->now() + adevs::Time(world_time, 0);
	}

	adevs::Time HealthGPS::update(adevs::SimEnv<int>* env)
	{
		if (env->now() < end_time_) {
			auto start = std::chrono::steady_clock::now();

			// Update mortality data, mortality is forward looking 2009 is for 2008-2009
			demographic_->update_residual_mortality(context_, *disease_);

			// Now move world clock to time t + 1
			auto world_time = env->now() + adevs::Time(1, 0);
			auto time_year = world_time.real;
			context_.set_current_time(time_year);

			update_population();
			std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;

			auto message = std::format("[{:4},{}], elapsed: {}ms",
				env->now().real, env->now().logical, elapsed.count());
			context_.publish(std::make_unique<InfoEventMessage>(
				name(), ModelAction::update, context_.current_run(), context_.time_now(), message));

			// Schedule next event time 
			return world_time;
		}

		// We have reached the end, remove the model and return infinite time for next event.
		env->remove(this);
		return adevs_inf<adevs::Time>();
	}

	adevs::Time HealthGPS::update(adevs::SimEnv<int>*, std::vector<int>&)
	{
		// This method is never called because nobody sends messages.
		return adevs_inf<adevs::Time>();
	}

	void HealthGPS::fini(adevs::Time clock)
	{
		auto message = std::format("[{:4},{}] clear up resources.", clock.real, clock.logical);
		context_.publish(std::make_unique<InfoEventMessage>(
			name(), ModelAction::stop, context_.current_run(), context_.time_now(), message));
	}

	void HealthGPS::initialise_population()
	{
		/* Note: order is very important */

		// Create virtual population
		auto model_start_year = config_.start_time();
		auto total_year_pop_size = demographic_->get_total_population(model_start_year);
		auto virtual_pop_size = static_cast<int>(config_.settings().size_fraction() * total_year_pop_size);
		context_.reset_population(virtual_pop_size, model_start_year);

		// Gender - Age, must be first
		demographic_->initialise_population(context_);

		// Social economics status
		ses_->initialise_population(context_);

		// Generate risk factors
		risk_factor_->initialise_population(context_);

		// Initialise diseases
		disease_->initialise_population(context_);

		// Initialise analysis
		analysis_->initialise_population(context_);

		print_initial_population_statistics();
	}

	void HealthGPS::update_population()
	{
		/* Note: order is very important */

		// update basic information: demographics + diseases
		update_age_and_lifecycle_events();

		// update population socio-economic status
		ses_->update_population(context_);

		// initialise risk factors for newborns and updates population risk factors
		risk_factor_->update_population(context_);

		// Calculate the net immigration by gender and age, update the population accordingly
		// TODO: move into demographics module?
		update_net_immigration();

		// Update diseases status: remission and incidence
		disease_->update_population(context_);

		// Publish results to data logger
		analysis_->update_population(context_);
	}

	void hgps::HealthGPS::update_age_and_lifecycle_events() {
		auto initial_pop_size = context_.population().current_active_size();
		auto expected_pop_size = demographic_->get_total_population(context_.time_now());
		auto expected_num_deaths = demographic_->get_total_deaths(context_.time_now());

		// apply death events
		auto number_of_deaths = update_age_and_death_events();

		// Update basic information (not included in death counts? -> fold in above);
		auto max_age = static_cast<unsigned int>(context_.age_range().upper());
		for (auto& entity : context_.population()) {
			if (!entity.is_active()) {
				continue;
			}

			entity.age = entity.age + 1;
			if (entity.age >= max_age) {
				entity.is_alive = false;
				entity.time_of_death = context_.time_now();
			}
		}

		// apply births events
		auto last_year_births_rate = demographic_->get_birth_rate(context_.time_now() - 1);
		auto number_of_boys = static_cast<int> (last_year_births_rate.male * initial_pop_size);
		auto number_of_girls = static_cast<int>(last_year_births_rate.female * initial_pop_size);
		context_.population().add_newborn_babies(number_of_boys, core::Gender::male);
		context_.population().add_newborn_babies(number_of_girls, core::Gender::female);

		// Calculate debug statistics, to be removed.
		auto number_of_births = number_of_boys + number_of_girls;
		auto expected_migration = (expected_pop_size / 100.0) - initial_pop_size - number_of_births + number_of_deaths;

		auto middle_pop_size = std::count_if(context_.population().cbegin(),
			context_.population().cend(), [](const auto& p) { return p.is_active(); });

		double pop_size_diff = (expected_pop_size / 100.0) - middle_pop_size;
		double simulated_death_rate = number_of_deaths * 1000.0 / initial_pop_size;
		double expected_death_rate = expected_num_deaths * 1000.0 / expected_pop_size;
		double percentage_diff = 100 * (simulated_death_rate / expected_death_rate - 1);
	}

	int HealthGPS::update_age_and_death_events() {
		auto max_age = static_cast<unsigned int>(context_.age_range().upper());
		auto number_of_deaths = 0;
		for (auto& entity : context_.population()) {
			if (!entity.is_active()) {
				continue;
			}

			if (entity.age >= max_age) {
				entity.is_alive = false;
				entity.time_of_death = context_.time_now();
				number_of_deaths++;
			}
			else {

				// calculate death probability based on the health status
				auto death_rate = demographic_->get_residual_death_rate(entity.age, entity.gender);
				auto product = 1.0 - death_rate;
				for (const auto& item : entity.diseases) {
					if (item.second.status == DiseaseStatus::active) {
						auto excess_mortality = disease_->get_excess_mortality(item.first, entity.age, entity.gender);
						product *= 1.0 - excess_mortality;
					}
				}

				auto death_probability = 1.0 - product;
				auto hazard = context_.next_double();
				if (hazard < death_probability) {
					entity.is_alive = false;
					entity.time_of_death = context_.time_now();
					number_of_deaths++;
				}
			}
		}

		return number_of_deaths;
	}

	void HealthGPS::update_net_immigration() {
		auto expected_population = get_current_expected_population();
		auto simulated_population = get_current_simulated_population();

		// Debug counters, remove on final version
		auto male_count = 0;
		auto female_count = 0;
		auto net_diff = 0;

		auto net_immigration = create_age_gender_table<int>(context_.age_range());
		auto start_age = context_.age_range().lower();
		auto end_age = context_.age_range().upper();
		for (int age = start_age; age <= end_age; age++) {
			net_diff = expected_population.at(age,
				core::Gender::male) - simulated_population.at(age, core::Gender::male);
			net_immigration.at(age, core::Gender::male) = net_diff;
			male_count += net_diff;

			net_diff = expected_population.at(age,
				core::Gender::female) - simulated_population.at(age, core::Gender::female);
			net_immigration.at(age, core::Gender::female) = net_diff;
			female_count += net_diff;
		}

		// Debug only, remove on final version
		auto total_immigration = male_count + female_count;

		// Update population based on net immigration
		for (int age = start_age; age <= end_age; age++) {
			auto male_net_value = net_immigration.at(age, core::Gender::male);
			apply_net_migration(male_net_value, age, core::Gender::male);

			auto female_net_value = net_immigration.at(age, core::Gender::female);
			apply_net_migration(female_net_value, age, core::Gender::female);
		}
	}

	hgps::IntegerAgeGenderTable HealthGPS::get_current_expected_population() const {
		auto sim_start_time = context_.reference_time();
		auto total_initial_population = demographic_->get_total_population(sim_start_time);
		auto start_population_size = static_cast<int>(config_.settings().size_fraction() * total_initial_population);

		auto current_population_table = demographic_->get_population(context_.time_now());
		auto expected_population = create_age_gender_table<int>(context_.age_range());
		auto start_age = context_.age_range().lower();
		auto end_age = context_.age_range().upper();
		for (int age = start_age; age <= end_age; age++) {
			auto age_info = current_population_table.at(age);
			expected_population.at(age, core::Gender::male) =
				static_cast<int>(std::round(age_info.males * start_population_size / total_initial_population));

			expected_population.at(age, core::Gender::female) =
				static_cast<int>(std::round(age_info.females * start_population_size / total_initial_population));
		}

		return expected_population;
	}

	hgps::IntegerAgeGenderTable HealthGPS::get_current_simulated_population() {
		auto simulated_population = create_age_gender_table<int>(context_.age_range());
		for (const auto& entity : context_.population()) {
			if (!entity.is_active()) {
				continue;
			}

			simulated_population.at(entity.age, entity.gender)++;
		}

		return simulated_population;
	}

	std::vector<std::reference_wrapper<const Person>> HealthGPS::get_similar_entities(
		const int& age, const core::Gender& gender)
	{
		auto similar_entities = std::vector<std::reference_wrapper<const Person>>();
		for (const auto& entity : context_.population()) {
			if (!entity.is_active()) {
				continue;
			}

			if (entity.age == age && entity.gender == gender) {
				similar_entities.push_back(entity);
			}
		}

		return similar_entities;
	}

	void HealthGPS::apply_net_migration(int net_value, int& age, const core::Gender& gender) {
		if (net_value > 0) {
			auto similar_entities = get_similar_entities(age, gender);
			if (similar_entities.size() > 0) {
				for (auto trial = 0; trial < net_value; trial++) {
					auto index = context_.next_int(static_cast<int>(similar_entities.size()) - 1);
					auto& source = similar_entities.at(index).get();
					context_.population().add(std::move(partial_clone_entity(source)));
				}
			}
		}
		else if (net_value < 0) {
			auto net_value_counter = net_value;
			for (auto& entity : context_.population()) {
				if (!entity.is_active()) {
					continue;
				}

				if (entity.age == age && entity.gender == gender) {
					entity.has_emigrated = true;
					net_value_counter++;
					if (net_value_counter == 0) {
						break;
					}
				}
			}
		}
	}

	Person HealthGPS::partial_clone_entity(const Person& source) const noexcept {
		auto clone = Person{};
		clone.age = source.age;
		clone.gender = source.gender;
		clone.education.set_both_values(source.education.value());
		clone.income.set_both_values(source.income.value());
		clone.is_alive = true;
		clone.has_emigrated = false;
		for (const auto& item : source.risk_factors) {
			clone.risk_factors.emplace(item.first, item.second);
		}

		for (const auto& item : source.diseases) {
			clone.diseases.emplace(item.first, item.second.clone());
		}

		return clone;
	}

	void hgps::HealthGPS::print_initial_population_statistics()
	{
		// TODO: Move to the analytics module
		auto visitor = UnivariateVisitor();
		auto orig_summary = std::unordered_map<std::string, core::UnivariateSummary>();
		auto sim8_summary = std::unordered_map<std::string, core::UnivariateSummary>();
		for (auto& entry : context_.mapping()) {
			config_.data().column(entry.name())->accept(visitor);
			orig_summary.emplace(entry.name(), visitor.get_summary());
			sim8_summary.emplace(entry.name(), core::UnivariateSummary(entry.name()));
		}

		for (auto& entity : context_.population()) {
			for (auto& entry : context_.mapping()) {
				sim8_summary[entry.name()].append(entity.get_risk_factor_value(entry.entity_key()));
			}
		}

		std::size_t longestColumnName = 0;
		for (auto& entry : context_.mapping()) {
			longestColumnName = std::max(longestColumnName, entry.name().length());
		}

		auto pad = longestColumnName + 2;
		auto width = pad + 70;
		auto orig_pop = config_.data().num_rows();
		auto sim8_pop = context_.population().size();

		std::stringstream ss;
		ss << std::format("\n Initial Virtual Population Summary:\n");
		ss << std::format("|{:-<{}}|\n", '-', width);
		ss << std::format("| {:{}} : {:>14} : {:>14} : {:>14} : {:>14} |\n",
			"Variable", pad, "Mean (Real)", "Mean (Sim)", "StdDev (Real)", "StdDev (Sim)");
		ss << std::format("|{:-<{}}|\n", '-', width);

		ss << std::format("| {:{}} : {:14} : {:14} : {:14} : {:14} |\n",
			"Population", pad, orig_pop, sim8_pop, orig_pop, sim8_pop);

		for (auto& entry : context_.mapping()) {
			auto col = entry.name();
			ss << std::format("| {:{}} : {:14.4f} : {:14.5f} : {:14.5f} : {:14.5f} |\n",
				col, pad, orig_summary[col].average(), sim8_summary[col].average(),
				orig_summary[col].std_deviation(), sim8_summary[col].std_deviation());
		}

		ss << std::format("|{:_<{}}|\n\n", '_', width);
		std::cout << ss.str();
	}
}