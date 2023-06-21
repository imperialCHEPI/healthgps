#pragma once

#include "simulation_definition.h"
#include <adevs/adevs.h>

namespace hgps {

/// @brief Defines the simulation class interface
class Simulation : public adevs::Model<int> {
  public:
    Simulation() = delete;
    /// @brief Initialises a new instance of the Simulation class
    /// @param definition The simulation configuration
    explicit Simulation(SimulationDefinition &&definition) : definition_{std::move(definition)} {}

    /// @brief Destroys a simulation instance
    virtual ~Simulation() = default;

    /// @brief Initialises the simulation experiment
    virtual void initialize() = 0;

    /// @brief Terminates the simulation experiment
    virtual void terminate() = 0;

    /// @brief Set-up a new simulation run with default seed
    /// @param run_number The run number
    virtual void setup_run(const unsigned int run_number) noexcept = 0;

    /// @brief Set-up a new simulation run
    /// @param run_number The run number
    /// @param run_seed The custom seed for random number generation
    virtual void setup_run(const unsigned int run_number, const unsigned int run_seed) noexcept = 0;

    /// @brief Gets the simulation type
    /// @return The intervention scenario type enumeration
    ScenarioType type() noexcept { return definition_.scenario().type(); }

    /// @brief Gets the simulation name
    /// @return The intervention scenario name
    std::string name() override { return definition_.identifier(); }

  protected:
    SimulationDefinition definition_;
};
} // namespace hgps