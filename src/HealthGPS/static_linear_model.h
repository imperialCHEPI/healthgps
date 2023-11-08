#pragma once

#include "interfaces.h"
#include "mapping.h"
#include "risk_factor_adjustable_model.h"

#include <Eigen/Dense>

namespace hgps {

/// @brief Defines the linear model parameters used to initialise risk factors
struct LinearModelParams {
    core::Identifier name;
    double intercept;
    std::unordered_map<core::Identifier, double> coefficients;
};

/// @brief Implements the static linear model type
///
/// @details The static model is used to initialise the virtual population.
class StaticLinearModel final : public RiskFactorAdjustableModel {
  public:
    /// @brief Initialises a new instance of the StaticLinearModel class
    /// @param risk_factor_expected The risk factor expected values by sex and age
    /// @param risk_factor_models The linear models used to initialise a person's risk factor values
    /// @param risk_factor_cholesky The Cholesky decomposition of the risk factor correlation matrix
    /// @throws HgpsException for invalid arguments
    StaticLinearModel(const RiskFactorSexAgeTable &risk_factor_expected,
                      const std::vector<LinearModelParams> &risk_factor_models,
                      const Eigen::MatrixXd &risk_factor_cholesky);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

    Eigen::VectorXd correlated_samples(RuntimeContext &context);

    void linear_approximation(Person &person);

    void initialise_weight(Person &person, Random generator);

    double get_weight_quantile(double energy_quantile, core::Gender gender, Random generator);

  private:
    const std::vector<LinearModelParams> &risk_factor_models_;
    const Eigen::MatrixXd &risk_factor_cholesky_;
};

/// @brief Defines the static linear model data type
class StaticLinearModelDefinition : public RiskFactorAdjustableModelDefinition {
  public:
    /// @brief Initialises a new instance of the StaticLinearModelDefinition class
    /// @param risk_factor_expected The risk factor expected values by sex and age
    /// @param risk_factor_models The linear models used to initialise a person's risk factor values
    /// @param risk_factor_cholesky The Cholesky decomposition of the risk factor correlation matrix
    /// @throws HgpsException for invalid arguments
    StaticLinearModelDefinition(RiskFactorSexAgeTable risk_factor_expected,
                                std::vector<LinearModelParams> risk_factor_models,
                                Eigen::MatrixXd risk_factor_cholesky);

    /// @brief Construct a new StaticLinearModel from this definition
    /// @return A unique pointer to the new StaticLinearModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::vector<LinearModelParams> risk_factor_models_;
    Eigen::MatrixXd risk_factor_cholesky_;
};

} // namespace hgps
