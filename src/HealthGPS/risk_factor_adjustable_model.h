#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include "map2d.h"
#include "risk_factor_model.h"
#include "runtime_context.h"

#include <functional>
#include <optional>
#include <unordered_set>
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
    /// @param trend_steps The number of time steps to apply the trend
    RiskFactorAdjustableModel(
        std::shared_ptr<RiskFactorSexAgeTable> expected,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps);

    /// @brief Gets a person's expected risk factor value
    /// @param context The simulation run-time context
    /// @param sex The sex key to get the expected value
    /// @param age The age key to get the expected value
    /// @param factor The risk factor to get the expected value
    /// @param range An optional expected value range
    /// @param apply_trend Whether to apply expected value time trend
    /// @returns The person's expected risk factor value
    virtual double get_expected(RuntimeContext &context, core::Gender sex, int age,
                                const core::Identifier &factor, OptionalRange range,
                                bool apply_trend) const noexcept;

    /// @brief Adjust risk factors such that mean sim value matches expected value
    /// @param context The simulation run-time context
    /// @param factors A list of risk factors to be adjusted
    /// @param ranges An optional list of risk factor value boundaries
    /// @param apply_trend Whether to apply expected value time trend
    void adjust_risk_factors(RuntimeContext &context, const std::vector<core::Identifier> &factors,
                             OptionalRanges ranges, bool apply_trend) const;

    /// @brief Gets the number of time steps to apply the trend
    /// @param factor The risk factor to get the trend steps
    /// @returns The number of time steps to apply the trend
    int get_trend_steps(const core::Identifier &factor) const;

  protected:
    /// @brief Set of factors that have logistic models for simulated mean calculation
    std::unordered_set<core::Identifier> logistic_factors_;

  private:
    /// @brief Adjust risk factors such that mean sim value matches expected value
    /// @param context The simulation run-time context
    /// @param factors A list of risk factors to be adjusted
    /// @param ranges An optional list of risk factor value boundaries
    /// @param apply_trend Whether to apply expected value time trend
    RiskFactorSexAgeTable calculate_adjustments(RuntimeContext &context,
                                                const std::vector<core::Identifier> &factors,
                                                OptionalRanges ranges, bool apply_trend) const;

    static RiskFactorSexAgeTable
    calculate_simulated_mean(Population &population, core::IntegerInterval age_range,
                             const std::vector<core::Identifier> &factors,
                             const std::unordered_set<core::Identifier> &logistic_factors);

    // MAHIMA: New overloaded function that can access stored calculation details
    static RiskFactorSexAgeTable
    calculate_simulated_mean_with_details(Population &population, core::IntegerInterval age_range,
                                         const std::vector<core::Identifier> &factors,
                                         const std::unordered_set<core::Identifier> &logistic_factors,
                                         const std::function<double(const Person&, const core::Identifier&)>& get_value_func);

    /// @brief Set logistic factors for simulated mean calculation
    /// @param logistic_factors Set of factors that have logistic models
    void set_logistic_factors(const std::unordered_set<core::Identifier> &logistic_factors);

    std::shared_ptr<RiskFactorSexAgeTable> expected_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps_;
};

/// @brief Risk factor adjustable model definition interface
class RiskFactorAdjustableModelDefinition : public RiskFactorModelDefinition {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModelDefinition instance
    /// @param expected The expected risk factor values by sex and age
    /// @param expected_trend The expected trend of risk factor values
    /// @param trend_steps The number of time steps to apply the trend
    /// @throws HgpsException for invalid arguments
    RiskFactorAdjustableModelDefinition(
        std::unique_ptr<RiskFactorSexAgeTable> expected,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps);

  protected:
    std::shared_ptr<RiskFactorSexAgeTable> expected_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps_;
};

} // namespace hgps
