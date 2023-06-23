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

/// @brief Defines the lite hierarchical linear model data type
class LiteHierarchicalModelDefinition final {
  public:
    LiteHierarchicalModelDefinition() = delete;

    /// @brief Initialises a new instance of the LiteHierarchicalModelDefinition class
    /// @param equations The linear regression equations
    /// @param variables The factors delta variables mapping
    /// @param boundary_percentage The boundary percentage to sample
    /// @throws std::invalid_argument for empty model equations definition
    LiteHierarchicalModelDefinition(
        std::map<core::IntegerInterval, AgeGroupGenderEquation> &&equations,
        std::map<core::Identifier, core::Identifier> &&variables,
        const double boundary_percentage = 0.05);

    /// @brief Gets the model factors' delta variables mapping
    /// @return Delta variables mapping
    const std::map<core::Identifier, core::Identifier> &variables() const noexcept;

    /// @brief Gets the linear regression equations for a given age
    /// @param age The target age
    /// @return The model equations
    const AgeGroupGenderEquation &at(const int &age) const;

    /// @brief Gets the boundary percentage to sample
    /// @return Boundary percentage value
    const double &boundary_percentage() const;

  private:
    std::map<core::IntegerInterval, AgeGroupGenderEquation> equations_;
    std::map<core::Identifier, core::Identifier> variables_;
    double boundary_percentage_;
};

/// @brief Implements the dynamic hierarchical linear model (energy balance) type
///
/// @details The dynamic model is used to advance the virtual population over time.
class EnergyBalanceHierarchicalModel final : public HierarchicalLinearModel {
  public:
    /// @brief Initialises a new instance of the EnergyBalanceHierarchicalModel class
    /// @param definition The model definition instance
    EnergyBalanceHierarchicalModel(LiteHierarchicalModelDefinition &definition);

    HierarchicalModelType type() const noexcept override;

    const std::string &name() const noexcept override;

    /// @copydoc HierarchicalLinearModel::generate_risk_factors
    /// @throws std::logic_error the dynamic model does not generate risk factors.
    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    std::reference_wrapper<LiteHierarchicalModelDefinition> definition_;
    std::string name_{"Dynamic"};

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

} // namespace hgps