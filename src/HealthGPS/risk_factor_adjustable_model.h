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

/// @brief Defines the trend type enumeration for factors mean adjustment
enum class TrendType {
    Null,       ///< No trends applied to factors mean adjustment
    Trend,      ///< Regular UPF trends applied to factors mean adjustment
    IncomeTrend ///< Income-based trends applied to factors mean adjustment
};

/// @brief Defines a table type for double values by sex and age
using RiskFactorSexAgeTable = UnorderedMap2d<core::Gender, core::Identifier, std::vector<double>>;

/// @brief Risk factor model interface with mean adjustment by sex and age
class RiskFactorAdjustableModel : public RiskFactorModel {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModel instance
    /// @param expected The risk factor expected values by sex and age
    /// @param expected_trend The expected trend of risk factor values (for UPF trends)
    /// @param trend_steps The number of time steps to apply the trend (for UPF trends)
    /// @param trend_type The type of trend to apply to factors mean adjustment
    /// @param expected_income_trend The expected income trend of risk factor values
    /// @param expected_income_trend_decay_factors The exponential decay factors for income trends
    RiskFactorAdjustableModel(
        std::shared_ptr<RiskFactorSexAgeTable> expected,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
        TrendType trend_type = TrendType::Null,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend =
            nullptr,
        std::shared_ptr<std::unordered_map<core::Identifier, double>>
            expected_income_trend_decay_factors = nullptr);

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
                             const std::vector<core::Identifier> &factors);

    std::shared_ptr<RiskFactorSexAgeTable> expected_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps_;

    // Trend type for factors mean adjustment
    TrendType trend_type_;

    // Income trend data structures (only used when trend_type_ == TrendType::IncomeTrend)
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>>
        expected_income_trend_decay_factors_;
};

/// @brief Risk factor adjustable model definition interface
class RiskFactorAdjustableModelDefinition : public RiskFactorModelDefinition {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModelDefinition instance
    /// @param expected The expected risk factor values by sex and age
    /// @param expected_trend The expected trend of risk factor values
    /// @param trend_steps The number of time steps to apply the trend
    /// @param trend_type The type of trend to apply (optional, defaults to Null)
    /// @throws HgpsException for invalid arguments
    RiskFactorAdjustableModelDefinition(
        std::unique_ptr<RiskFactorSexAgeTable> expected,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
        TrendType trend_type = TrendType::Null);

  protected:
    std::shared_ptr<RiskFactorSexAgeTable> expected_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps_;
    TrendType trend_type_;
};

} // namespace hgps
