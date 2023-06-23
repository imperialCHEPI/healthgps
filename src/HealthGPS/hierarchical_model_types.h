#pragma once
#include "gender_value.h"
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

/// @brief Defines the full hierarchical linear model data type
class HierarchicalLinearModelDefinition final {
  public:
    HierarchicalLinearModelDefinition() = delete;
    /// @brief Initialises a new instance of the HierarchicalLinearModelDefinition class
    /// @param linear_models The linear regression models equations
    /// @param model_levels The hierarchical model levels definition
    /// @throws std::invalid_argument for empty regression models equations or hierarchical model
    /// levels
    HierarchicalLinearModelDefinition(
        std::unordered_map<core::Identifier, LinearModel> &&linear_models,
        std::map<int, HierarchicalLevel> &&model_levels)
        : models_{std::move(linear_models)}, levels_{std::move(model_levels)} {
        if (models_.empty()) {
            throw std::invalid_argument(
                "The hierarchical model equations definition must not be empty");
        }

        if (levels_.empty()) {
            throw std::invalid_argument(
                "The hierarchical model levels definition must not be empty");
        }
    }

    /// @brief Gets the hierarchical model's linear regression equations
    /// @return Linear regression equations
    const std::unordered_map<core::Identifier, LinearModel> &models() const noexcept {
        return models_;
    }

    /// @brief Gets the hierarchical model's levels definitions
    /// @return Hierarchical model levels
    const std::map<int, HierarchicalLevel> &levels() const noexcept { return levels_; }

  private:
    std::unordered_map<core::Identifier, LinearModel> models_;
    std::map<int, HierarchicalLevel> levels_;
};

} // namespace hgps
