#pragma once

#include "HealthGPS.Core/identifier.h"

#include "mapping.h"
#include "random_algorithm.h"
#include "risk_factor_adjustable_model.h"

namespace hgps {

/// @brief Defines a factor linear regression equation
struct FactorDynamicEquation {
    /// @brief The factor name
    std::string name;

    /// @brief The regression model coefficients
    std::map<core::Identifier, double> coefficients{};

    /// @brief The residuals standard deviation
    double residuals_standard_deviation{};
};

/// @brief Define the age group dynamic equations data type
struct AgeGroupGenderEquation {
    /// @brief The reference age group
    core::IntegerInterval age_group;

    /// @brief The males dynamic equations
    std::map<core::Identifier, FactorDynamicEquation> male{};

    /// @brief The females dynamic equations
    std::map<core::Identifier, FactorDynamicEquation> female{};
};

/// @brief Implements the dynamic hierarchical linear model type
///
/// @details The dynamic model is used to advance the virtual population over time.
class DynamicHierarchicalLinearModel final : public RiskFactorAdjustableModel {
  public:
    /// @brief Initialises a new instance of the DynamicHierarchicalLinearModel class
    /// @param expected The expected values
    /// @param expected_trend The expected trend of risk factor values
    /// @param trend_steps The number of time steps to apply the trend
    /// @param equations The linear regression equations
    /// @param variables The factors delta variables mapping
    /// @param boundary_percentage The boundary percentage to sample
    DynamicHierarchicalLinearModel(
        std::shared_ptr<RiskFactorSexAgeTable> expected,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
        const std::map<core::IntegerInterval, AgeGroupGenderEquation> &equations,
        const std::map<core::Identifier, core::Identifier> &variables,
        const double boundary_percentage);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    const std::map<core::IntegerInterval, AgeGroupGenderEquation> &equations_;
    const std::map<core::Identifier, core::Identifier> &variables_;
    double boundary_percentage_;

    const AgeGroupGenderEquation &equations_at(int age) const;

    void update_risk_factors_exposure(
        RuntimeContext &context, Person &entity,
        const std::map<core::Identifier, double> &current_risk_factors,
        const std::map<core::Identifier, FactorDynamicEquation> &equations);

    static std::map<core::Identifier, double>
    get_current_risk_factors(const HierarchicalMapping &mapping, Person &entity);

    double sample_normal_with_boundary(Random &random, double mean, double standard_deviation,
                                       double boundary) const;
};

/// @brief Defines the dynamic hierarchical linear model data type
class DynamicHierarchicalLinearModelDefinition : public RiskFactorAdjustableModelDefinition {
  public:
    /// @brief Initialises a new instance of the DynamicHierarchicalLinearModelDefinition class
    /// @param expected The expected values
    /// @param expected_trend The expected trend of risk factor values
    /// @param trend_steps The number of time steps to apply the trend
    /// @param equations The linear regression equations
    /// @param variables The factors delta variables mapping
    /// @param boundary_percentage The boundary percentage to sample
    /// @throws std::invalid_argument for empty model equations definition
    DynamicHierarchicalLinearModelDefinition(
        std::unique_ptr<RiskFactorSexAgeTable> expected,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
        std::map<core::IntegerInterval, AgeGroupGenderEquation> equations,
        std::map<core::Identifier, core::Identifier> variables,
        const double boundary_percentage = 0.05);

    /// @brief Construct a new DynamicHierarchicalLinearModel from this definition
    /// @return A unique pointer to the new DynamicHierarchicalLinearModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::map<core::IntegerInterval, AgeGroupGenderEquation> equations_;
    std::map<core::Identifier, core::Identifier> variables_;
    double boundary_percentage_;
};

} // namespace hgps
