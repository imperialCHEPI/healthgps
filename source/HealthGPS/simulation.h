#pragma once

#include <vector>
#include "adevs/adevs.h"
#include "interfaces.h"
#include "scenario.h"

namespace hgps {

	class Simulation : public adevs::Model<int>
	{
	public:
		Simulation() = delete;
		explicit Simulation(Scenario& scenario);
		explicit Simulation(Scenario& scenario, RandomBitGenerator&& generator);
		
		adevs::Time init(adevs::SimEnv<int>* env);

		adevs::Time update(adevs::SimEnv<int>* env);

		adevs::Time update(adevs::SimEnv<int>*, std::vector<int>&);

		void fini(adevs::Time clock);

		double next_double() const;
	private:
		RandomBitGenerator& rnd_;
		Scenario scenario_;
		adevs::Time end_time_;
	};
}