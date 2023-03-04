#pragma once

#include "modelinput.h"
#include "scenario.h"
#include "randombit_generator.h"

#include <functional>
#include <memory>

namespace hgps {

	/// @brief Simulation experiment definition data type
	struct SimulationDefinition {
		SimulationDefinition() = delete;

		/// @brief Initialises a new instance of the SimulationDefinition structure
		/// @param inputs The simulation model inputs
		/// @param scenario The scenario to simulate
		/// @param generator The pseudo-random number generator instance
		SimulationDefinition(ModelInput& inputs, std::unique_ptr<Scenario> scenario,
			std::unique_ptr<RandomBitGenerator> generator);

		/// @brief Gets a read-only reference to the model inputs definition
		/// @return The model inputs reference
		const ModelInput& inputs() const noexcept;

		/// @brief Gets a reference to the simulation scenario instance
		/// @return Experiment Scenario reference
		Scenario& scenario() noexcept;

		/// @brief Gets a reference to the pseudo-random number generator instance
		/// @return Pseudo-random number generator reference
		RandomBitGenerator& rnd() noexcept;

		/// @brief Gets the simulation scenario identifier 
		/// @return The simulation identifier
		const std::string& identifier() const noexcept;

	private:
		std::reference_wrapper<ModelInput> inputs_;
		std::unique_ptr<Scenario> scenario_;
		std::unique_ptr<RandomBitGenerator> rnd_;
		std::string identifier_;
	};
}
