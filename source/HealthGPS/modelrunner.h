#pragma once

#include "modulefactory.h"
#include "simulation.h"
#include "scenario.h"
#include <HealthGPS/modelinput.h>

namespace hgps {

	class ModelRunner
	{
	public:
		ModelRunner() = delete;
		explicit ModelRunner(SimulationModuleFactory& factory, ModelInput& config);

		double run(Simulation& model, const unsigned int trial_runs) const;

	private:
		ModelInput config_;
		SimulationModuleFactory factory_;
	};
}