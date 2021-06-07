#pragma once

#include <vector>
#include <adevs/adevs.h>

#include "interfaces.h"
#include "scenario.h"

namespace hgps {

	class HealthGPS : public adevs::Model<int>
	{
	public:
		HealthGPS() = delete;
		explicit HealthGPS(Scenario& scenario, RandomBitGenerator&& generator);
		
		adevs::Time init(adevs::SimEnv<int>* env);

		adevs::Time update(adevs::SimEnv<int>* env);

		adevs::Time update(adevs::SimEnv<int>*, std::vector<int>&);

		void fini(adevs::Time clock);

		double next_double();
	private:
		RandomBitGenerator& rnd_;
		Scenario scenario_;
		adevs::Time end_time_;
	};
}