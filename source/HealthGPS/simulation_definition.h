#pragma once

#include "modelinput.h"
#include "scenario.h"
#include "randombit_generator.h"

#include <functional>
#include <memory>

namespace hgps {

	struct SimulationDefinition {
		SimulationDefinition() = delete;
		SimulationDefinition(ModelInput& inputs, std::unique_ptr<Scenario> scenario,
			std::unique_ptr<RandomBitGenerator> generator);

		const ModelInput& inputs() const noexcept;

		Scenario& scenario() noexcept;

		RandomBitGenerator& rnd() noexcept;

		std::string identifier() const noexcept;

	private:
		std::reference_wrapper<ModelInput> inputs_;
		std::unique_ptr<Scenario> scenario_;
		std::unique_ptr<RandomBitGenerator> rnd_;
		std::string identifier_;
	};
}
