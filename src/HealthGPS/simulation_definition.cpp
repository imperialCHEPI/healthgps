#include "simulation_definition.h"

namespace hgps {
	SimulationDefinition::SimulationDefinition(ModelInput& inputs,
		std::unique_ptr<Scenario> scenario, std::unique_ptr<RandomBitGenerator> generator)
		: inputs_{ inputs }, scenario_{ std::move(scenario) }, rnd_{ std::move(generator) },
		identifier_{ core::to_lower(scenario_->name()) } {}

	const ModelInput& SimulationDefinition::inputs() const noexcept {
		return inputs_;
	}

	Scenario& SimulationDefinition::scenario() noexcept {
		return *scenario_;
	}

	RandomBitGenerator& SimulationDefinition::rnd() noexcept {
		return *rnd_;
	}

	std::string SimulationDefinition::identifier() const noexcept {
		return identifier_;
	}
}