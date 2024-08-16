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

using OptionalRange = std::optional<std::reference_wrapper<const hgps::core::DoubleInterval>>;

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
    /// @param expected_trend The expected trend of risk factor values
    RiskFactorAdjustableModel(
        std::shared_ptr<RiskFactorSexAgeTable> expected,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend);

    /// @brief Gets a person's expected risk factor value
    /// @param context The simulation run-time context
    /// @param sex The sex key to get the expected value
    /// @param age The age key to get the expected value
    /// @param factor The risk factor to get the expected value
    /// @param range An optional expected value range
    /// @returns The person's expected risk factor value
    double get_expected(RuntimeContext &context, core::Gender sex, int age,
                        const core::Identifier &factor,
                        OptionalRange range = std::nullopt) const noexcept;

    /// @brief Adjust risk factors such that mean sim value matches expected value
    /// @param context The simulation run-time context
    /// @param factors A list of risk factors to be adjusted
    /// @param ranges An optional list of risk factor value boundaries
    void adjust_risk_factors(RuntimeContext &context, const std::vector<core::Identifier> &factors,
                             OptionalRanges ranges = std::nullopt) const;

  private:
    /// @brief Adjust risk factors such that mean sim value matches expected value
    /// @param context The simulation run-time context
    /// @param factors A list of risk factors to be adjusted
    /// @param ranges An optional list of risk factor value boundaries
    RiskFactorSexAgeTable calculate_adjustments(RuntimeContext &context,
                                                const std::vector<core::Identifier> &factors,
                                                OptionalRanges ranges) const;

    static RiskFactorSexAgeTable
    calculate_simulated_mean(Population &population, core::IntegerInterval age_range,
                             const std::vector<core::Identifier> &factors);

    std::shared_ptr<RiskFactorSexAgeTable> expected_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
};

/// @brief Risk factor adjustable model definition interface
class RiskFactorAdjustableModelDefinition : public RiskFactorModelDefinition {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModelDefinition instance
    /// @param expected The expected risk factor values by sex and age
    /// @param expected_trend The expected trend of risk factor values
    /// @throws HgpsException for invalid arguments
    RiskFactorAdjustableModelDefinition(
        std::unique_ptr<RiskFactorSexAgeTable> expected,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend);

  protected:
    std::shared_ptr<RiskFactorSexAgeTable> expected_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
};

} // namespace hgps
