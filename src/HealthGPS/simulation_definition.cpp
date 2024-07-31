#include "simulation_definition.h"

namespace hgps {

SimulationDefinition::SimulationDefinition(ModelInput &inputs, std::unique_ptr<Scenario> scenario)
    : inputs_{inputs}, scenario_{std::move(scenario)},
      identifier_{core::to_lower(scenario_->name())} {}

const ModelInput &SimulationDefinition::inputs() const noexcept { return inputs_; }

Scenario &SimulationDefinition::scenario() noexcept { return *scenario_; }

const std::string &SimulationDefinition::identifier() const noexcept { return identifier_; }

} // namespace hgps
