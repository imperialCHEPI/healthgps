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
		auto ref_year = config_.settings().reference_time();
		auto pop_year = demographic_->get_total_population(ref_year);
		auto pop_size = static_cast<int>(config_.settings().size_fraction() * pop_year);

		auto world_time = config_.start_time();
		context_.set_current_time(world_time);
		initialise_population(pop_size, ref_year);

		auto message = std::format("[{:4},{}] population size: {}",
			env->now().real, env->now().logical, pop_size);
		context_.publish(InfoEventMessage{ name(), ModelAction::start,
			context_.current_run(), context_.time_now(), message });

		return env->now() + adevs::Time(world_time, 0);
	}

	adevs::Time HealthGPS::update(adevs::SimEnv<int>* env)
	{
		if (env->now() < end_time_) {
			auto start = std::chrono::steady_clock::now();
			auto ref_year = config_.settings().reference_time();
			auto pop_year = demographic_->get_total_population(ref_year);
			auto initial_pop_size = static_cast<int>(config_.settings().size_fraction() * pop_year);

			// Update mortality data, mortality is forward looking 2009 is for 2008-2009
			demographic_->update_residual_mortality(context_, *disease_);

			// Now move world clock to time t + 1
			auto world_time = env->now() + adevs::Time(1, 0);
			auto time_year = world_time.real;
			context_.set_current_time(time_year);

			update_population(initial_pop_size);

			std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;

			auto message = std::format("[{:4},{}] elapsed: {}ms",
				env->now().real, env->now().logical, elapsed.count());
			context_.publish(InfoEventMessage{ name(), ModelAction::update,
				context_.current_run(), context_.time_now(), message });

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
		context_.publish(InfoEventMessage{ name(), ModelAction::stop,
			context_.current_run(), context_.time_now(), message });
	}

	void HealthGPS::initialise_population(const int pop_size, const int ref_year)
	{
		// Note the order is very important
		context_.reset_population(pop_size, ref_year);

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

		// Print out initial population statistics
		// 
		// TODO: Move to the analytics module
		// 
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

	void HealthGPS::update_population(const int initial_pop_size) {

		// update basic information
		update_age_and_lifecycle_events();

		// update SES status - move into SES module
		update_socioeconomic_status();

		// update risk factors - move into risk factor module
		update_risk_factors();

		// Calculate the net immigration by gender and age, update the population accordingly
		update_net_immigration();

		// Update diseases remission and new cases

		// Publish results to data logger
		analysis_->update_population(context_);
	}

	void hgps::HealthGPS::update_age_and_lifecycle_events() {
		auto initial_pop_size = std::count_if(context_.population().cbegin(),
			context_.population().cend(), [](const auto& p) { return p.is_active(); });

		auto expected_pop_size = demographic_->get_total_population(context_.time_now());
		auto expected_num_deaths = demographic_->get_total_deaths(context_.time_now());
		
		// apply death events
		auto number_of_deaths =	update_age_and_death_events();

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
		// Apply deaths events
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
					if (item.second.status == DiseaseStatus::Active) {
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

	void HealthGPS::update_socioeconomic_status() {
		// Education
		auto male_education_table = ses_->get_education_frequency(core::Gender::male);
		auto female_education_table = ses_->get_education_frequency(core::Gender::female);
		auto education_levels = std::vector<int>(male_education_table.begin()->second.size());
		std::iota(education_levels.begin(), education_levels.end(), 0);

		// Income
		auto male_income_table = ses_->get_income_frenquency(core::Gender::male);
		auto female_income_table = ses_->get_income_frenquency(core::Gender::female);
		auto number_of_levels = male_income_table.begin()->second.columns();
		auto income_levels = std::vector<int>(number_of_levels);
		std::iota(income_levels.begin(), income_levels.end(), 0);

		auto education_freq = std::vector<float>{};
		auto income_freq = std::vector<float>{};
		for (auto& entity : context_.population()) {
			if (!entity.is_active()) {
				continue;
			}

			if (entity.gender == core::Gender::male) {
				education_freq = male_education_table.at(entity.age);
				income_freq = std::vector<float>(number_of_levels);
				for (int level = 0; level < number_of_levels; level++) {
					income_freq[level] = male_income_table.at(entity.age)(entity.education.value(), level);
				}
			}
			else {
				education_freq = female_education_table.at(entity.age);
				income_freq = std::vector<float>(number_of_levels);
				for (int level = 0; level < number_of_levels; level++) {
					income_freq[level] = female_income_table.at(entity.age)(entity.education.value(), level);
				}
			}

			update_education_level(entity, education_levels, education_freq);
			update_income_level(entity, income_levels, income_freq);
		}
	}

	void hgps::HealthGPS::update_education_level(Person& entity,
		std::vector<int>& education_levels, std::vector<float>& education_freq) {
		auto education_cdf = detail::create_cdf(education_freq);
		auto random_education_level = context_.next_empirical_discrete(education_levels, education_cdf);

		// Very important to retrieve the old education level when calculating
		// the risk value from the previous year.
		if (entity.education.value() == 0) {
			entity.education.set_both_values(random_education_level);
		}
		else {
			entity.education = entity.education.value();

			// To prevent education level getting maximal values ???
			if (entity.age % 5 == 0 && entity.age < 31) {
				entity.education = std::max(entity.education.value(), random_education_level);
			}
		}
	}

	void HealthGPS::update_income_level(Person& entity,
		std::vector<int>& income_levels, std::vector<float>& income_freq) {

		auto income_cdf = detail::create_cdf(income_freq);
		auto random_income_Level = context_.next_empirical_discrete(income_levels, income_cdf);
		if (entity.income.value() == 0) {
			entity.income.set_both_values(random_income_Level);
		}
		else {
			entity.income = entity.income.value();

			// To prevent education level getting maximal values?, < ? mine just collapsed
			if (entity.age % 5 == 0 && entity.age < 31) {
				entity.income = std::max(entity.income.value(), random_income_Level);
			}
		}
	}

	void HealthGPS::update_risk_factors() {
		
		// TODO: risk_factor_.update_population(context_)
		auto dynamic_model = risk_factor_->operator[](HierarchicalModelType::Dynamic);

		// Generate risk factors for newborns
		dynamic_model->generate_risk_factors(context_);

		// Update risk factors for population
		dynamic_model->update_risk_factors(context_);
	}

	void HealthGPS::update_net_immigration()	{
		auto sim_start_time = config_.start_time();
		auto total_initial_population = demographic_->get_total_population(sim_start_time);
		auto start_population_size = static_cast<int>(config_.settings().size_fraction() * total_initial_population);

		// GetExpectedMalePopulation
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

		// GetSimulatedMalePopulation
		auto simulated_population = create_age_gender_table<int>(context_.age_range());
		for (auto& entity : context_.population()) {
			if (!entity.is_active()) {
				continue;
			}

			simulated_population.at(entity.age, entity.gender)++;
		}

		auto net_immigration = create_age_gender_table<int>(context_.age_range());
		auto male_count = 0;
		auto female_count = 0;
		auto net_diff = 0;
		for (int age = start_age; age <= end_age; age++) {
			net_diff = expected_population.at(age, core::Gender::male) - simulated_population.at(age, core::Gender::male);
			net_immigration.at(age, core::Gender::male) = net_diff;
			male_count += net_diff;

			net_diff = expected_population.at(age, core::Gender::female) - simulated_population.at(age, core::Gender::female);
			net_immigration.at(age, core::Gender::female) = net_diff;
			female_count += net_diff;
		}

		// Debug only, remove on final version
		auto total_immigration = male_count + female_count;

		// Update population based on net immigration
		for (int age = start_age; age <= end_age; age++) {

			// Male
			auto male_net_value = net_immigration.at(age, core::Gender::male);
			if (male_net_value > 0) {
				// Get list of similar individuals
				auto similar_males = std::vector<std::reference_wrapper<const Person>>();
				for (const auto& entity : context_.population()) {
					if (!entity.is_active()) {
						continue;
					}

					if (entity.age == age && entity.gender == core::Gender::male) {
						similar_males.push_back(entity);
					}
				}

				if (similar_males.size() > 0) {
					for (size_t i = 0; i < male_net_value; i++) {
						auto index = context_.next_int(static_cast<int>(similar_males.size()) - 1);
						auto& source = similar_males.at(index).get();
						context_.population().add(std::move(clone_entiry(source)));
					}
				}
			}
			else if (male_net_value < 0) {

				for (auto& entity : context_.population()) {
					if (!entity.is_active()) {
						continue;
					}

					if (entity.age == age && entity.gender == core::Gender::male) {
						entity.has_emigrated = true;
						male_net_value++;
						if (male_net_value == 0) {
							break;
						}
					}
				}
			}

			// Female
			auto female_net_value = net_immigration.at(age, core::Gender::female);
			if (female_net_value > 0) {
				// Get list of similar individuals
				auto similar_females = std::vector<std::reference_wrapper<const Person>>();
				for (const auto& entity : context_.population()) {
					if (!entity.is_active()) {
						continue;
					}

					if (entity.age == age && entity.gender == core::Gender::female) {
						similar_females.push_back(entity);
					}
				}

				if (similar_females.size() > 0) {
					for (size_t i = 0; i < female_net_value; i++) {
						auto index = context_.next_int(static_cast<int>(similar_females.size()) - 1);
						auto& source = similar_females.at(index).get();
						context_.population().add(std::move(clone_entiry(source)));
					}
				}
			}
			else if (female_net_value < 0) {
				for (auto& entity : context_.population()) {
					if (!entity.is_active()) {
						continue;
					}

					if (entity.age == age && entity.gender == core::Gender::female) {
						entity.has_emigrated = true;
						female_net_value++;
						if (female_net_value == 0) {
							break;
						}
					}
				}
			}
		}
	}

	Person HealthGPS::clone_entiry(const Person& source) const noexcept {
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
}