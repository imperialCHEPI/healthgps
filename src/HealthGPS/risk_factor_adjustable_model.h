#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include "map2d.h"
#include "risk_factor_model.h"
#include "runtime_context.h"

namespace hgps {

/// @brief Defines a table type for double values by sex and age
using RiskFactorSexAgeTable = UnorderedMap2d<core::Gender, core::Identifier, std::vector<double>>;

/// @brief Risk factor model interface with mean adjustment by sex and age
class RiskFactorAdjustableModel : public RiskFactorModel {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModel instance
    /// @param risk_factor_expected The expected risk factor values by sex and age
    RiskFactorAdjustableModel(const RiskFactorSexAgeTable &risk_factor_expected);

    /// @brief Gets the expected risk factor values by sex and age
    /// @returns The expected risk factor values by sex and age
    const RiskFactorSexAgeTable &get_risk_factor_expected() const noexcept;

    /// @brief Adjust ALL risk factors such that mean simulated value matches expected value
    /// @param context The simulation run-time context
    virtual void adjust_risk_factors(RuntimeContext &context) const;

    // TODO: for SOME risk factors
    // TODO: names should be a std::unordered_set for faster inclusion check

    // /// @brief Adjust SOME risk factors such that mean simulated value matches expected value
    // /// @param context The simulation run-time context
    // /// @param names The list of risk factors to be adjusted
    // virtual void adjust_risk_factors(RuntimeContext &context,
    //                                  const std::set<core::Identifier> &names) const;

  private:
    RiskFactorSexAgeTable get_adjustments(RuntimeContext &context) const;

    RiskFactorSexAgeTable calculate_adjustments(RuntimeContext &context) const;

    static RiskFactorSexAgeTable calculate_simulated_mean(RuntimeContext &context);

    const RiskFactorSexAgeTable &risk_factor_expected_;
};

} // namespace hgps
