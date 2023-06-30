#pragma once

#include "interfaces.h"
#include "mapping.h"

namespace hgps {

/// @brief Regression coefficients data type
struct Coefficient {
    /// @brief The coefficient value
    double value{};

    /// @brief Associated p-value
    double pvalue{};

    /// @brief Associated t-value
    double tvalue{};

    /// @brief Associated standard error
    double std_error{};
};

/// @brief Defines a linear regression model data type
struct LinearModel {
    /// @brief The model coefficients
    std::unordered_map<core::Identifier, Coefficient> coefficients;

    /// @brief The residuals standard deviation value
    double residuals_standard_deviation;

    /// @brief The R squared value
    double rsquared{};
};

/// @brief Defines a hierarchical level data type
struct HierarchicalLevel {

    /// @brief The level variables
    std::unordered_map<core::Identifier, int> variables;

    /// @brief The transition matrix
    core::DoubleArray2D transition;

    /// @brief The inverse transition matrix
    core::DoubleArray2D inverse_transition;

    /// @brief The residuals distribution
    core::DoubleArray2D residual_distribution;

    /// @brief The correlation matrix
    core::DoubleArray2D correlation;

    /// @brief The associated variance
    std::vector<double> variances;
};

/// @brief Implements the static hierarchical linear model type
///
/// @details The static model is used to initialise the virtual population,
/// the model uses principal component analysis for residual normalisation.
class StaticHierarchicalLinearModel final : public HierarchicalLinearModel {
  public:
    /// @brief Initialises a new instance of the StaticHierarchicalLinearModel class
    /// @param linear_models The linear regression models equations
    /// @param model_levels The hierarchical model levels definition
    StaticHierarchicalLinearModel(const std::unordered_map<core::Identifier, LinearModel> &models,
                                  const std::map<int, HierarchicalLevel> &levels);

    HierarchicalModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    std::reference_wrapper<const std::unordered_map<core::Identifier, LinearModel>> models_;
    std::reference_wrapper<const std::map<int, HierarchicalLevel>> levels_;

    void generate_for_entity(RuntimeContext &context, Person &entity, int level,
                             std::vector<MappingEntry> &level_factors);
};

/// @brief Defines the full hierarchical linear model data type
class HierarchicalLinearModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the HierarchicalLinearModelDefinition class
    /// @param linear_models The linear regression models equations
    /// @param model_levels The hierarchical model levels definition
    /// @throws std::invalid_argument for empty arguments
    HierarchicalLinearModelDefinition(
        std::unordered_map<core::Identifier, LinearModel> linear_models,
        std::map<int, HierarchicalLevel> model_levels);

    /// @brief Construct a new StaticHierarchicalLinearModel from this definition
    /// @return A unique pointer to the new StaticHierarchicalLinearModel instance
    std::unique_ptr<HierarchicalLinearModel> create_model() const override;

  private:
    std::unordered_map<core::Identifier, LinearModel> models_;
    std::map<int, HierarchicalLevel> levels_;
};

} // namespace hgps
