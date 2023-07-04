#pragma once

#include "interfaces.h"
#include "mapping.h"
#include "random_algorithm.h"

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

/// @brief Implements the dynamic hierarchical linear model (energy balance) type
///
/// @details The dynamic model is used to advance the virtual population over time.
class EnergyBalanceHierarchicalModel final : public HierarchicalLinearModel {
  public:
    /// @brief Initialises a new instance of the EnergyBalanceHierarchicalModel class
    /// @param equations The linear regression equations
    /// @param variables The factors delta variables mapping
    /// @param boundary_percentage The boundary percentage to sample
    EnergyBalanceHierarchicalModel(
        const std::map<core::IntegerInterval, AgeGroupGenderEquation> &equations,
        const std::map<core::Identifier, core::Identifier> &variables,
        const double boundary_percentage);

    HierarchicalModelType type() const noexcept override;

    std::string name() const noexcept override;

    /// @copydoc HierarchicalLinearModel::generate_risk_factors
    /// @throws std::logic_error the dynamic model does not generate risk factors.
    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    const std::map<core::IntegerInterval, AgeGroupGenderEquation> &equations_;
    const std::map<core::Identifier, core::Identifier> &variables_;
    double boundary_percentage_;

    const AgeGroupGenderEquation &equations_at(const int &age) const;

    void update_risk_factors_exposure(
        RuntimeContext &context, Person &entity,
        const std::map<core::Identifier, double> &current_risk_factors,
        const std::map<core::Identifier, FactorDynamicEquation> &equations);

    std::map<core::Identifier, double> get_current_risk_factors(const HierarchicalMapping &mapping,
                                                                Person &entity,
                                                                int time_year) const;

    double sample_normal_with_boundary(Random &random, double mean, double standard_deviation,
                                       double boundary) const;
};

/// @brief Defines the lite hierarchical linear model data type
class LiteHierarchicalModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the LiteHierarchicalModelDefinition class
    /// @param equations The linear regression equations
    /// @param variables The factors delta variables mapping
    /// @param boundary_percentage The boundary percentage to sample
    /// @throws std::invalid_argument for empty model equations definition
    LiteHierarchicalModelDefinition(
        std::map<core::IntegerInterval, AgeGroupGenderEquation> equations,
        std::map<core::Identifier, core::Identifier> variables,
        const double boundary_percentage = 0.05);

    /// @brief Construct a new EnergyBalanceHierarchicalModel from this definition
    /// @return A unique pointer to the new EnergyBalanceHierarchicalModel instance
    std::unique_ptr<HierarchicalLinearModel> create_model() const override;

  private:
    std::map<core::IntegerInterval, AgeGroupGenderEquation> equations_;
    std::map<core::Identifier, core::Identifier> variables_;
    double boundary_percentage_;
};

} // namespace hgps
