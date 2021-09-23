#pragma once

#include <adevs/adevs.h>
#include "simulation_definition.h"

namespace hgps {

	class Simulation : public adevs::Model<int>
	{
	public:
		Simulation() = delete;
		explicit Simulation(SimulationDefinition&& definition)
			: definition_{std::move(definition)} {}

		virtual ~Simulation() = default;

		virtual void initialize() = 0;

		virtual void terminate() = 0;

		virtual void set_current_run(const unsigned int run_number) noexcept = 0;

		ScenarioType type() noexcept { return definition_.scenario().type(); }

	protected:
		SimulationDefinition definition_;
	};
}