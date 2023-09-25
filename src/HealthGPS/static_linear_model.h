#pragma once

#include "interfaces.h"
#include "mapping.h"

#include <Eigen/Dense>

namespace hgps {

/// @brief Defines the linear model parameters used to initialise risk factors
struct LinearModelParams {
    std::unordered_map<core::Identifier, double> intercepts;
    std::unordered_map<core::Identifier, std::unordered_map<core::Identifier, double>> coefficients;
};

/// @brief Implements the static linear model type
///
/// @details The static model is used to initialise the virtual population.
class StaticLinearModel final : public HierarchicalLinearModel {
  public:
    /// @brief Initialises a new instance of the StaticLinearModel class
    /// @param risk_factor_names An ordered list of risk factor names
    /// @param risk_factor_models The linear models used to initialise a person's risk factor values
    /// @param risk_factor_cholesky The Cholesky decomposition of the risk factor correlation matrix
    StaticLinearModel(std::vector<core::Identifier> risk_factor_names,
                      LinearModelParams risk_factor_models, Eigen::MatrixXd risk_factor_cholesky);

    HierarchicalModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    const std::vector<core::Identifier> risk_factor_names_;
    const LinearModelParams risk_factor_models_;
    const Eigen::MatrixXd risk_factor_cholesky_;
};

/// @brief Defines the static linear model data type
class StaticLinearModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the StaticLinearModelDefinition class
    /// @param risk_factor_names An ordered list of risk factor names
    /// @param risk_factor_models The linear models used to initialise a person's risk factor values
    /// @param risk_factor_cholesky The Cholesky decomposition of the risk factor correlation matrix
    /// @throws std::invalid_argument for empty arguments
    StaticLinearModelDefinition(std::vector<core::Identifier> risk_factor_names,
                                LinearModelParams risk_factor_models,
                                Eigen::MatrixXd risk_factor_cholesky);

    /// @brief Construct a new StaticLinearModel from this definition
    /// @return A unique pointer to the new StaticLinearModel instance
    std::unique_ptr<HierarchicalLinearModel> create_model() const override;

  private:
    std::vector<core::Identifier> risk_factor_names_;
    LinearModelParams risk_factor_models_;
    Eigen::MatrixXd risk_factor_cholesky_;
};

} // namespace hgps
