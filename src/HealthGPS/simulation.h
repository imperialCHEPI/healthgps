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

		virtual void setup_run(const unsigned int run_number) noexcept = 0;

		virtual void setup_run(const unsigned int run_number, const unsigned int run_seed) noexcept = 0;

		ScenarioType type() noexcept { return definition_.scenario().type(); }

		std::string name() override { return definition_.identifier(); }

	protected:
		SimulationDefinition definition_;
	};
}