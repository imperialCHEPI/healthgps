#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include "map2d.h"
#include "risk_factor_model.h"
#include "runtime_context.h"

#include <functional>
#include <optional>
#include <vector>

namespace { // anonymous namespace

using OptionalRanges =
    std::optional<std::reference_wrapper<const std::vector<hgps::core::DoubleInterval>>>;

} // anonymous namespace

namespace hgps {

/// @brief Defines a table type for double values by sex and age
using RiskFactorSexAgeTable = UnorderedMap2d<core::Gender, core::Identifier, std::vector<double>>;

/// @brief Risk factor model interface with mean adjustment by sex and age
class RiskFactorAdjustableModel : public RiskFactorModel {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModel instance
    /// @param expected The risk factor expected values by sex and age
    RiskFactorAdjustableModel(const RiskFactorSexAgeTable &expected);

    /// @brief Gets the risk factor expected values by sex and age
    /// @returns The risk factor expected values by sex and age
    const RiskFactorSexAgeTable &get_expected() const noexcept;

    /// @brief Adjust risk factors such that mean sim value matches expected value
    /// @param context The simulation run-time context
    /// @param factors A list of risk factors to be adjusted
    /// @param ranges An optional list of risk factor value boundaries
    void adjust_risk_factors(RuntimeContext &context, const std::vector<core::Identifier> &factors,
                             OptionalRanges ranges = std::nullopt) const;

  private:
    RiskFactorSexAgeTable calculate_adjustments(RuntimeContext &context,
                                                const std::vector<core::Identifier> &factors) const;

    static RiskFactorSexAgeTable
    calculate_simulated_mean(Population &population, core::IntegerInterval age_range,
                             const std::vector<core::Identifier> &factors);

  protected:
    const RiskFactorSexAgeTable &expected_;
};

/// @brief Risk factor adjustable model definition interface
class RiskFactorAdjustableModelDefinition : public RiskFactorModelDefinition {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModelDefinition instance
    /// @param expected The expected risk factor values by sex and age
    /// @throws HgpsException for invalid arguments
    RiskFactorAdjustableModelDefinition(RiskFactorSexAgeTable expected);

  protected:
    RiskFactorSexAgeTable expected_;
};

} // namespace hgps
