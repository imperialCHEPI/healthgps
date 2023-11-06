#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include "map2d.h"
#include "risk_factor_model.h"
#include "runtime_context.h"

#include <unordered_set>
namespace hgps {

/// @brief Defines a table type for double values by sex and age
using RiskFactorSexAgeTable = UnorderedMap2d<core::Gender, core::Identifier, std::vector<double>>;

/// @brief Risk factor model interface with mean adjustment by sex and age
class RiskFactorAdjustableModel : public RiskFactorModel {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModel instance
    /// @param risk_factor_expected The risk factor expected values by sex and age
    RiskFactorAdjustableModel(const RiskFactorSexAgeTable &risk_factor_expected);

    /// @brief Gets the risk factor expected values by sex and age
    /// @returns The risk factor expected values by sex and age
    const RiskFactorSexAgeTable &get_risk_factor_expected() const noexcept;

    /// @brief Adjust risk factors such that mean sim value matches expected value
    /// @param context The simulation run-time context
    /// @param keys A set of keys for risk factors to be adjusted
    void adjust_risk_factors(RuntimeContext &context,
                             const std::unordered_set<core::Identifier> &keys) const;

  private:
    RiskFactorSexAgeTable
    calculate_adjustments(RuntimeContext &context,
                          const std::unordered_set<core::Identifier> &keys) const;

    static RiskFactorSexAgeTable
    calculate_simulated_mean(RuntimeContext &context,
                             const std::unordered_set<core::Identifier> &keys);

    const RiskFactorSexAgeTable &risk_factor_expected_;
};

/// @brief Risk factor adjustable model definition interface
class RiskFactorAdjustableModelDefinition : public RiskFactorModelDefinition {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModelDefinition instance
    /// @param risk_factor_expected The expected risk factor values by sex and age
    /// @throws HgpsException for invalid arguments
    RiskFactorAdjustableModelDefinition(RiskFactorSexAgeTable risk_factor_expected);

    /// @brief Gets the risk factor expected values by sex and age
    /// @returns The risk factor expected values by sex and age
    const RiskFactorSexAgeTable &get_risk_factor_expected() const noexcept;

  private:
    RiskFactorSexAgeTable risk_factor_expected_;
};

} // namespace hgps
