#pragma once

#include "modelinput.h"
#include "scenario.h"

#include <functional>
#include <memory>

namespace hgps {

/// @brief Simulation experiment definition data type
struct SimulationDefinition {
    SimulationDefinition() = delete;

    /// @brief Initialises a new instance of the SimulationDefinition structure
    /// @param inputs The simulation model inputs
    /// @param scenario The scenario to simulate
    SimulationDefinition(ModelInput &inputs, std::unique_ptr<Scenario> scenario);

    /// @brief Gets a read-only reference to the model inputs definition
    /// @return The model inputs reference
    const ModelInput &inputs() const noexcept;

    /// @brief Gets a reference to the simulation scenario instance
    /// @return Experiment Scenario reference
    Scenario &scenario() noexcept;

    /// @brief Gets the simulation scenario identifier
    /// @return The simulation identifier
    const std::string &identifier() const noexcept;

  private:
    std::reference_wrapper<ModelInput> inputs_;
    std::unique_ptr<Scenario> scenario_;
    std::string identifier_;
};

} // namespace hgps
