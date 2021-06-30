#include <iostream>
#include <chrono>
#include <thread>

#include "healthgps.h"
#include "mtrandom.h"

namespace hgps {
	HealthGPS::HealthGPS(SimulationModuleFactory& factory, ModelInput& config, RandomBitGenerator&& generator)
		: Simulation(config, std::move(generator)), factory_{ factory }, context_{ rnd_ } {

		// Create required modules, should change to shared_ptr
		auto ses_base = factory.Create(SimulationModuleType::SES, config_);
		auto dem_base = factory.Create(SimulationModuleType::Demographic, config_);

		ses_.reset(dynamic_cast<SESModule*>(ses_base.release()));
		demographic_.reset(dynamic_cast<DemographicModule*>(dem_base.release()));
	}

	void HealthGPS::initialize()
	{
		// Reset random number generator
		if (config_.seed().has_value()){
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
		// Reset runt-me context;
		auto ref_year = config_.settings().reference_time();
		auto pop_year = demographic_->get_total_population(ref_year);
		auto pop_size = (int)(config_.settings().size_fraction() * pop_year);
		context_.reset_population(pop_size);
		context_.set_current_time(config_.start_time());

		std::cout << "Started @ " << env->now().real << "," << env->now().logical
			      << ", population size: " << pop_size << std::endl;

		return env->now() + adevs::Time(config_.start_time(), 0);
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
			auto clock = env->now() + adevs::Time(1, 0);
			context_.set_current_time(clock.real);
			return clock;
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
		std::cout << "Finished @ " << clock.real << "," << clock.logical 
				  << ", clear up memory." << std::endl;
	}
}