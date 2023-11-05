#pragma once

#include "risk_factor_adjustable_model.h"
#include "runtime_context.h"

namespace hgps {

/// @brief Defines the baseline risk factors adjustment model.
class RiskfactorAdjustmentModel {
  public:
    RiskfactorAdjustmentModel() = delete;
    /// @brief Initialises a new instance of the RiskfactorAdjustmentModel class
    /// @param risk_factor_expected The expected risk factor values
    RiskfactorAdjustmentModel(RiskFactorSexAgeTable &risk_factor_expected);

    /// @brief Applies the baseline adjustments to the population risk factor values
    /// @param context The simulation shared runtime context instance
    /// @throws std::runtime_error for simulation instances out of synchronisation or message
    /// timeout.
    void Apply(RuntimeContext &context);

  private:
    RiskFactorSexAgeTable get_adjustments(RuntimeContext &context) const;

    RiskFactorSexAgeTable calculate_adjustments(RuntimeContext &context) const;

    static RiskFactorSexAgeTable calculate_simulated_mean(RuntimeContext &context);

    std::reference_wrapper<RiskFactorSexAgeTable> risk_factor_expected_;
};

} // namespace hgps
