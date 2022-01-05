#include "healthgps.h"
#include "mtrandom.h"
#include "univariate_visitor.h"
#include "info_message.h"
#include "converter.h"
#include "hierarchical_model.h"
#include "baseline_sync_message.h"

#include <iostream>
#include <algorithm>
#include <numeric>
#include <memory>
#include <fmt/format.h>

namespace hgps {
	HealthGPS::HealthGPS(SimulationDefinition&& definition, SimulationModuleFactory& factory, EventAggregator& bus)
		: Simulation(std::move(definition)), context_{ bus, definition_ }
	{
		// Create required modules, should change to shared_ptr
		auto ses_base = factory.create(SimulationModuleType::SES, definition_.inputs());
		auto dem_base = factory.create(SimulationModuleType::Demographic, definition_.inputs());
		auto risk_base = factory.create(SimulationModuleType::RiskFactor, definition_.inputs());
		auto disease_base = factory.create(SimulationModuleType::Disease, definition_.inputs());
		auto analysis_base = factory.create(SimulationModuleType::Analysis, definition_.inputs());

		ses_ = std::static_pointer_cast<UpdatableModule>(ses_base);
		demographic_ = std::static_pointer_cast<DemographicModule>(dem_base);
		risk_factor_ = std::static_pointer_cast<RiskFactorHostModule>(risk_base);
		disease_ = std::static_pointer_cast<DiseaseHostModule>(disease_base);
		analysis_ = std::static_pointer_cast<UpdatableModule>(analysis_base);
	}

	void HealthGPS::initialize()
	{
		// Reset random number generator
		if (definition_.inputs().seed().has_value()) {
			definition_.rnd().seed(definition_.inputs().seed().value());
		}

		end_time_ = adevs::Time(definition_.inputs().stop_time(), 0);
		std::cout << "Microsimulation algorithm initialised: " << name() << std::endl;
	}

	void HealthGPS::terminate() {
		std::cout << "Microsimulation algorithm terminate: " << name() << std::endl;
	}

	void HealthGPS::setup_run(const unsigned int run_number) noexcept {
		context_.set_current_run(run_number);
	}

	void HealthGPS::setup_run(const unsigned int run_number, const unsigned int run_seed) noexcept {
		context_.set_current_run(run_number);
		definition_.rnd().seed(run_seed);
	}

	adevs::Time HealthGPS::init(adevs::SimEnv<int>* env)
	{
		auto start = std::chrono::steady_clock::now();
		auto world_time = definition_.inputs().start_time();
		context_.metrics().clear();
		context_.scenario().clear();
		context_.set_current_time(world_time);
	
		initialise_population();

		auto stop = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration<double, std::milli>(stop - start);

		auto message = fmt::format("[{:4},{}] population size: {}, elapsed: {}ms",
			env->now().real, env->now().logical, context_.population().initial_size(), elapsed.count());
		context_.publish(std::make_unique<InfoEventMessage>(
			name(), ModelAction::start, context_.current_run(), context_.time_now(), message));

		return env->now() + adevs::Time(world_time, 0);
	}

	adevs::Time HealthGPS::update(adevs::SimEnv<int>* env)
	{
		if (env->now() < end_time_) {
			auto start = std::chrono::steady_clock::now();
			context_.metrics().reset();

			// Update mortality data, mortality is forward looking 2009 is for 2008-2009
			demographic_->update_residual_mortality(context_, *disease_);

			// Now move world clock to time t + 1
			auto world_time = env->now() + adevs::Time(1, 0);
			auto time_year = world_time.real;
			context_.set_current_time(time_year);

			update_population();

			auto stop = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration<double, std::milli>(stop - start);

			auto message = fmt::format("[{:4},{}], elapsed: {}ms",
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
		auto bmi_value = 0.0;
		auto male_data = std::vector<double>{};
		auto feme_data = std::vector<double>{};
		auto male_info = core::UnivariateSummary("Male BMI");
		auto feme_info = core::UnivariateSummary("Female BMI");
		for (const auto& p : context_.population()) {
			if (!p.is_active()) {
				continue;
			}

			bmi_value = p.get_risk_factor_value("bmi");
			if (p.gender == core::Gender::male) {
				male_info.append(bmi_value);
				male_data.push_back(bmi_value);
			}
			else {
				feme_info.append(bmi_value);
				feme_data.push_back(bmi_value);
			}
		}

		auto male = male_info.to_string();
		auto feme = feme_info.to_string();

		// risk_factor_->update_population(context_);
		auto message = fmt::format("[{:4},{}] clear up resources.", clock.real, clock.logical);
		context_.publish(std::make_unique<InfoEventMessage>(
			name(), ModelAction::stop, context_.current_run(), context_.time_now(), message));
	}

	void HealthGPS::initialise_population()
	{
		/* Note: order is very important */

		// Create virtual population
		auto model_start_year = definition_.inputs().start_time();
		auto total_year_pop_size = demographic_->get_total_population_size(model_start_year);
		auto virtual_pop_size = static_cast<int>(definition_.inputs().settings().size_fraction() * total_year_pop_size);
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
		demographic_->update_population(context_, *disease_);

		// update population socio-economic status
		ses_->update_population(context_);

		// initialise risk factors for newborns and updates population risk factors
		risk_factor_->update_population(context_);

		// Calculate the net immigration by gender and age, update the population accordingly
		update_net_immigration();

		// Update diseases status: remission and incidence
		disease_->update_population(context_);

		// Publish results to data logger
		analysis_->update_population(context_);
	}

	void HealthGPS::update_net_immigration() {
		auto net_immigration = get_net_migration();

		// Update population based on net immigration
		auto start_age = context_.age_range().lower();
		auto end_age = context_.age_range().upper();
		for (int age = start_age; age <= end_age; age++) {
			auto male_net_value = net_immigration.at(age, core::Gender::male);
			apply_net_migration(male_net_value, age, core::Gender::male);

			auto female_net_value = net_immigration.at(age, core::Gender::female);
			apply_net_migration(female_net_value, age, core::Gender::female);
		}

		if (context_.scenario().type() == ScenarioType::baseline) {
			context_.scenario().channel().send(std::make_unique<NetImmigrationMessage>(
				context_.current_run(), context_.time_now(), std::move(net_immigration)));
		}
	}

	hgps::IntegerAgeGenderTable HealthGPS::get_current_expected_population() const {
		auto sim_start_time = context_.start_time();
		auto total_initial_population = demographic_->get_total_population_size(sim_start_time);
		auto start_population_size = static_cast<int>(definition_.inputs().settings().size_fraction() * total_initial_population);

		auto current_population_table = demographic_->get_population_distribution(context_.time_now());
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

	const std::vector<Person> HealthGPS::get_similar_entities(
		const int& age, const core::Gender& gender)
	{
		auto similar_entities = std::vector<Person>();
		for (const auto& entity : context_.population()) {
			if (!entity.is_active()) {
				continue;
			}

			if (entity.age == age && entity.gender == gender) {
				similar_entities.push_back(entity);
			}
		}

		similar_entities.shrink_to_fit();
		return similar_entities;
	}

	void HealthGPS::apply_net_migration(int net_value, int& age, const core::Gender& gender) {
		if (net_value > 0) {
			auto similar_entities = get_similar_entities(age, gender);
			if (similar_entities.size() > 0) {
				for (auto trial = 0; trial < net_value; trial++) {
					auto index = context_.random().next_int(static_cast<int>(similar_entities.size()) - 1);
					const auto& source = similar_entities.at(index);
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

	hgps::IntegerAgeGenderTable HealthGPS::get_net_migration() {
		if (context_.scenario().type() == ScenarioType::baseline) {
			return create_net_migration();
		}

		// Receive message with timeout
		auto message = context_.scenario().channel().try_receive(context_.sync_timeout_millis());
		if (message.has_value()) {
			auto& basePtr = message.value();
			auto messagePrt = dynamic_cast<NetImmigrationMessage*>(basePtr.get());
			if (messagePrt) {
				return messagePrt->data();
			}

			throw std::runtime_error(
				"Simulation out of sync, failed to receive a net immigration message");
		}
		else {
			throw std::runtime_error(
				"Simulation out of sync, receive net immigration message has timed out");
		}
	}

	hgps::IntegerAgeGenderTable HealthGPS::create_net_migration() {
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

		// Update statistics
		auto total_immigration = male_count + female_count;
		return net_immigration;
	}

	Person HealthGPS::partial_clone_entity(const Person& source) const noexcept {
		auto clone = Person{};
		clone.age = source.age;
		clone.gender = source.gender;
		clone.ses = source.ses;
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
		for (const auto& entry : context_.mapping()) {
			definition_.inputs().data().column(entry.name())->accept(visitor);
			orig_summary.emplace(entry.name(), visitor.get_summary());
			sim8_summary.emplace(entry.name(), core::UnivariateSummary(entry.name()));
		}

		for (auto& entity : context_.population()) {
			for (auto& entry : context_.mapping()) {
				sim8_summary[entry.name()].append(entity.get_risk_factor_value(entry.entity_key()));
			}
		}

		std::string population = "Population";
		std::size_t longestColumnName = population.length();
		for (const auto& entry : context_.mapping()) {
			longestColumnName = std::max(longestColumnName, entry.name().length());
		}

		auto pad = longestColumnName + 2;
		auto width = pad + 70;
		auto orig_pop = definition_.inputs().data().num_rows();
		auto sim8_pop = context_.population().size();

		std::stringstream ss;
		ss << fmt::format("\n Initial Virtual Population Summary: {}\n", context_.identifier());
		ss << fmt::format("|{:-<{}}|\n", '-', width);
		ss << fmt::format("| {:{}} : {:>14} : {:>14} : {:>14} : {:>14} |\n",
			"Variable", pad, "Mean (Real)", "Mean (Sim)", "StdDev (Real)", "StdDev (Sim)");
		ss << fmt::format("|{:-<{}}|\n", '-', width);

		ss << fmt::format("| {:{}} : {:14} : {:14} : {:14} : {:14} |\n",
			population, pad, orig_pop, sim8_pop, orig_pop, sim8_pop);

		for (const auto& entry : context_.mapping()) {
			auto col = entry.name();
			ss << fmt::format("| {:{}} : {:14.4f} : {:14.5f} : {:14.5f} : {:14.5f} |\n",
				col, pad, orig_summary[col].average(), sim8_summary[col].average(),
				orig_summary[col].std_deviation(), sim8_summary[col].std_deviation());
		}

		ss << fmt::format("|{:_<{}}|\n\n", '_', width);
		std::cout << ss.str();
	}
}