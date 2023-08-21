#pragma once

#include "riskfactor_adjustment_types.h"
#include "runtime_context.h"

#include <functional>

namespace hgps {

/// @brief Defines the baseline risk factors adjustment model.
class RiskfactorAdjustmentModel {
  public:
    RiskfactorAdjustmentModel() = delete;
    /// @brief Initialises a new instance of the BaselineAdjustment class
    /// @param adjustments The baseline adjustment definition
    RiskfactorAdjustmentModel(BaselineAdjustment &adjustments);

    /// @brief Applies the baseline adjustments to the population risk factor values
    /// @param context The simulation shared runtime context instance
    /// @throws std::runtime_error for simulation instances out of synchronisation or message
    /// timeout.
    void Apply(RuntimeContext &context);

  private:
    std::reference_wrapper<BaselineAdjustment> adjustments_;

    static FactorAdjustmentTable calculate_simulated_mean(Population &population,
                                                          const core::IntegerInterval &age_range);

    FactorAdjustmentTable calculate_adjustment_coefficients(RuntimeContext &context) const;

    FactorAdjustmentTable get_adjustment_coefficients(RuntimeContext &context) const;
};
} // namespace hgps
