#include <iostream>
#include <algorithm>
#include <numeric>

#include "healthgps.h"
#include "mtrandom.h"
#include "univariate_visitor.h"
#include "info_message.h"
#include "converter.h"

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

			std::uniform_int_distribution dist(100, 200);
			auto sleep_time = dist(rnd_);
			auto message = std::format("[{:4},{}] sleep: {}ms",
				env->now().real, env->now().logical, sleep_time);
			context_.publish(InfoEventMessage{ name(), ModelAction::update,
				context_.current_run(), context_.time_now(), message });

			std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

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
				for (int col = 0; col < number_of_levels; col++) {
					income_freq[col] = female_income_table.at(entity.age)(entity.education.value(), col);
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
			// To prevent education level getting maximal values ???
			auto current_education_level = static_cast<int>(entity.education.value());
			if (entity.age < 31 && current_education_level < random_education_level) {
				entity.education = random_education_level;
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
		else
		{
			// To prevent education level getting maximal values ???
			auto current_income_level = static_cast<int>(entity.income.value());
			if (entity.age < 31 && current_income_level < random_income_Level) {
				entity.income = random_income_Level;
			}
		}
	}
}