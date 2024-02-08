#pragma once

#include "risk_factor_model.h"

#include <string>
#include <vector>

namespace hgps {

/// @brief Implements the dummy risk factor model type
///
/// @details The dummy model is used to fix risk factors to known constant values.
class DummyModel final : public RiskFactorModel {
  public:
    /// @brief Initialises a new instance of the DummyModel class
    /// @param type Risk factor model type
    /// @param names Risk factor names
    /// @param values Constant values to set risk factors to
    /// @param policy Risk factor policy in intervention scenario
    /// @param policy_start Policy start (in years since the start of the simulation)
    DummyModel(RiskFactorModelType type, const std::vector<core::Identifier> &names,
               const std::vector<double> &values, const std::vector<double> &policy,
               const std::vector<int> &policy_start);

    /// @brief Get the risk factor model type
    /// @return The risk factor model type
    RiskFactorModelType type() const noexcept override;

    /// @brief Get the risk factor model name
    /// @return The risk factor model name
    std::string name() const noexcept override;

    /// @brief Initialise risk factor values and apply intervention policy
    /// @param context The runtime context
    void generate_risk_factors(RuntimeContext &context) override;

    /// @brief Update risk factor values and apply intervention policy
    /// @param context The runtime context
    void update_risk_factors(RuntimeContext &context) override;

  private:
    /// @brief Set risk factor values and apply intervention policy
    /// @param person The person to set the risk factors for
    /// @param scenario The scenario (either baseline or intervention)
    /// @param time_elapsed The number of years since the start of the simulation
    void set_risk_factors(Person &person, ScenarioType scenario, int time_elapsed) const;

    const RiskFactorModelType type_;
    const std::vector<core::Identifier> &names_;
    const std::vector<double> &values_;
    const std::vector<double> &policy_;
    const std::vector<int> &policy_start_;
};

/// @brief Defines the dummy risk factor model type
class DummyModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the DummyModelFactory class
    /// @param type Risk factor model type
    /// @param names Risk factor names
    /// @param values Constant values to set risk factors to
    /// @param policy Risk factor policy in intervention scenario
    /// @param policy_start Policy start (in years since the start of the simulation)
    DummyModelDefinition(RiskFactorModelType type, std::vector<core::Identifier> names,
                         std::vector<double> values, std::vector<double> policy,
                         std::vector<int> policy_start);

    /// @brief Construct a new DummyModel from this definition
    /// @return A unique pointer to the new DummyModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    RiskFactorModelType type_;
    std::vector<core::Identifier> names_;
    std::vector<double> values_;
    std::vector<double> policy_;
    std::vector<int> policy_start_;
};

} // namespace hgps
