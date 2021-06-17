#pragma once

#include "modulefactory.h"
#include "simulation.h"
#include "scenario.h"

namespace hgps {

	class ModelRunner
	{
	public:
		explicit ModelRunner(Simulation& model, SimulationModuleFactory& factory, const int trial_runs);

		ModelRunner() = delete;

		double run() const;

	private:
		const int trial_runs_;
		Simulation& simulation_;
		SimulationModuleFactory& factory_;
	};
}