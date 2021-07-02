#pragma once

#include "modulefactory.h"
#include "simulation.h"
#include "scenario.h"
#include <HealthGPS/modelinput.h>

namespace hgps {

	class ModelRunner
	{
	public:
		double run(Simulation& model, const unsigned int trial_runs) const;
	};
}