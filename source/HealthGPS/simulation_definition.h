#pragma once

#include "interfaces.h"
#include "modelinput.h"

namespace hgps {

	struct SimulationDefinition {
		SimulationDefinition() = delete;
		SimulationDefinition(ModelInput& inputs, Scenario&& scenario, RandomBitGenerator&& generator)
			: inputs_{ inputs }, scenario_{ scenario }, rnd_{ generator },
			identifier_{core::to_lower(scenario.name())} {}

		const ModelInput& inputs() const noexcept { return inputs_; }

		Scenario& scenario() noexcept { return scenario_; }

		RandomBitGenerator& rnd() { return rnd_; }

		std::string identifier() const noexcept {
			return identifier_;
		}

	private:
		ModelInput inputs_;
		Scenario& scenario_;
		RandomBitGenerator& rnd_;
		std::string identifier_;
	};
}
