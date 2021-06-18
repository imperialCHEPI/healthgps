#pragma once

#include "modulefactory.h"
#include "simulation.h"
#include "scenario.h"
#include <HealthGPS/modelcontext.h>

namespace hgps {

	class ModelRunner
	{
	public:
		ModelRunner() = delete;
		explicit ModelRunner(SimulationModuleFactory& factory, ModelContext& context);

		double run(Simulation& model, const unsigned int trial_runs) const;

	private:
		ModelContext context_;
		SimulationModuleFactory factory_;
	};
}