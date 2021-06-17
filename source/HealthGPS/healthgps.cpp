#include <iostream>
#include <chrono>
#include <thread>

#include "healthgps.h"
#include "mtrandom.h"

namespace hgps {
	HealthGPS::HealthGPS(Scenario& scenario, RandomBitGenerator&& generator)
		: Simulation(std::move(generator)), scenario_{ scenario }
	{
	}

	void HealthGPS::initialize() const
	{
		std::cout << "Microsimulation algorithm initialised." << std::endl;
	}

	void HealthGPS::terminate() const
	{
		std::cout << "Microsimulation algorithm terminate." << std::endl;
	}

	adevs::Time HealthGPS::init(adevs::SimEnv<int>* env)
	{
		end_time_ = adevs::Time(scenario_.get_stop_time(), 0);
		if (scenario_.custom_seed.has_value())
		{
			rnd_.seed(scenario_.custom_seed.value());
		}

		std::cout << "Started @ " << env->now().real << "," << env->now().logical
			      << ", allocate memory for population." << std::endl;

		return env->now() + adevs::Time(scenario_.get_start_time(), 0);
	}

	adevs::Time HealthGPS::update(adevs::SimEnv<int>* env)
	{
		std::uniform_int_distribution dist(100, 200);
		auto sleep_time = dist(rnd_);

		std::cout << "Update population @ " << env->now().real << "," 
			<< env->now().logical << " dt = " << sleep_time << "ms" << std::endl;

		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

		if (env->now() < end_time_)
		{
			return env->now() + adevs::Time(1, 0);
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

	double HealthGPS::next_double() { return rnd_.next_double(); }
}