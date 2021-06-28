#pragma once

#include <vector>

#include "simulation.h"
#include "scenario.h"

namespace hgps {

	class HealthGPS : public Simulation
	{
	public:
		HealthGPS() = delete;
		explicit HealthGPS(ModelInput& config, RandomBitGenerator&& generator);
		
		void initialize() const override;

		void terminate() const override;

		adevs::Time init(adevs::SimEnv<int>* env);

		adevs::Time update(adevs::SimEnv<int>* env);

		adevs::Time update(adevs::SimEnv<int>*, std::vector<int>&);

		void fini(adevs::Time clock);

		double next_double();
	private:

		//Scenario& scenario_;
	};
}