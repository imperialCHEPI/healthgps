#include <iostream>
#include <algorithm>
#include <numeric>

#include "healthgps.h"
#include "mtrandom.h"
#include "univariate_visitor.h"

namespace hgps {
	HealthGPS::HealthGPS(SimulationModuleFactory& factory, ModelInput& config, RandomBitGenerator&& generator)
		: Simulation(config, std::move(generator)), factory_{ factory },
		context_{ rnd_, config_.risk_mapping() } {

		// Create required modules, should change to shared_ptr
		auto ses_base = factory.create(SimulationModuleType::SES, config_);
		auto dem_base = factory.create(SimulationModuleType::Demographic, config_);
		auto risk_base = factory.create(SimulationModuleType::RiskFactor, config_);

		ses_ = std::dynamic_pointer_cast<SESModule>(ses_base);
		demographic_ = std::dynamic_pointer_cast<DemographicModule>(dem_base);
		risk_factor_ = std::dynamic_pointer_cast<RiskFactorModule>(risk_base);
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

	void HealthGPS::terminate()
	{
		std::cout << "Microsimulation algorithm terminate." << std::endl;
	}

	adevs::Time HealthGPS::init(adevs::SimEnv<int>* env)
	{
		auto ref_year = config_.settings().reference_time();
		auto pop_year = demographic_->get_total_population(ref_year);
		auto pop_size = (int)(config_.settings().size_fraction() * pop_year);

		initialise_population(pop_size, ref_year);

		auto world_time = config_.start_time();
		context_.set_current_time(world_time);
		std::cout << "Started @ " << context_.time_now() << " [" << env->now().real << ","
			<< env->now().logical << "], population size: " << pop_size << std::endl;
		return env->now() + adevs::Time(world_time, 0);
	}

	adevs::Time HealthGPS::update(adevs::SimEnv<int>* env)
	{
		std::uniform_int_distribution dist(100, 200);
		auto sleep_time = dist(rnd_);

		std::cout << "Update population @ " << context_.time_now() << " [" << env->now().real << ", "
			<< env->now().logical << "] dt = " << sleep_time << "ms" << std::endl;

		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

		if (env->now() < end_time_)
		{
			auto world_time = env->now() + adevs::Time(1, 0);
			context_.set_current_time(world_time.real);
			return world_time;
		}

		/* We have reached the end.
		   Return an infinite time of next event and remove the model. */
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
		std::cout << "Finished @ " << context_.time_now() << " [" << clock.real << ","
			<< clock.logical << "], clear up resources." << std::endl;
	}

	void HealthGPS::initialise_population(const int pop_size, const int ref_year)
	{
		// Note the order is very important
		context_.reset_population(pop_size);

		// Gender - Age, must be first
		demographic_->initialise_population(context_, ref_year);

		// Social economics status
		ses_->initialise_population(context_);

		// Generate risk factors
		risk_factor_->initialise_population(context_);

		// Print out initial population statistics
		// 
		// TODO: Move to the analytics module
		// 
		auto visitor = UnivariateVisitor();
		auto col_names = std::vector<std::string>{ "Gender", "Age", "Education", "Income" };
		auto orig_summary = std::unordered_map<std::string, core::UnivariateSummary>();
		auto sim8_summary = std::unordered_map<std::string, core::UnivariateSummary>();
		for (auto& name : col_names) {
			config_.data().column(name)->accept(visitor);
			orig_summary.emplace(name, visitor.get_summary());
			sim8_summary.emplace(name, core::UnivariateSummary(name));
		}

		for (auto& entity : context_.population()) {
			sim8_summary["Gender"].append(entity.gender == core::Gender::male ? 1.0 : 0.0);
			sim8_summary["Age"].append(entity.age);
			sim8_summary["Education"].append(entity.education);
			sim8_summary["Income"].append(entity.income);
		}

		std::size_t longestColumnName = 0;
		for (auto& col : col_names) {
			longestColumnName = std::max(longestColumnName, col.length());
		}

		auto pad = longestColumnName + 2;
		auto width = pad + 66;
		auto orig_pop = config_.data().num_rows();
		auto sim8_pop = context_.population().size();

		std::stringstream ss;
		ss << std::format("\n Initial Virtual Population Summary:\n");
		ss << std::format("|{:-<{}}|\n", '-', width);
		ss << std::format("| {:{}} : {:>12} : {:>12} : {:>14} : {:>14} |\n", 
			"Variable", pad, "Mean (Real)", "Mean (Sim)", "StdDev (Real)", "StdDev (Sim)");
		ss << std::format("|{:-<{}}|\n", '-', width);

		ss << std::format("| {:{}} : {:12} : {:12} : {:14} : {:14} |\n",
			"Population", pad, orig_pop, sim8_pop, orig_pop, sim8_pop);

		for (auto& col : col_names) {
			ss << std::format("| {:{}} : {:12.6f} : {:12.6f} : {:14.6f} : {:14.6f} |\n",
				col, pad, orig_summary[col].average(), sim8_summary[col].average(),
				orig_summary[col].std_deviation(), sim8_summary[col].std_deviation());
		}

		ss << std::format("|{:_<{}}|\n\n", '_', width);
		std::cout << ss.str();
	}
}