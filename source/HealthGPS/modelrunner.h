#pragma once

#include "modulefactory.h"
#include "simulation.h"
#include "scenario.h"
#include <HealthGPS/modelinput.h>
#include <HealthGPS/event_aggregator.h>

namespace hgps {

	class ModelRunner
	{
	public:
		ModelRunner() = delete;
		ModelRunner(EventAggregator& bus) noexcept;

		double run(Simulation& model, const unsigned int trial_runs) const;

	private:
		EventAggregator& event_bus_;
	};
}