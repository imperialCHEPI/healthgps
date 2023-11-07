#pragma once

#include "interfaces.h"
#include "mapping.h"
#include "risk_factor_adjustable_model.h"

#include <Eigen/Dense>

namespace hgps {

/// @brief Defines the linear model parameters used to initialise risk factors
struct LinearModelParams {
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
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param income_models The income models for each income category
    /// @throws HgpsException for invalid arguments
    StaticLinearModel(
        const RiskFactorSexAgeTable &risk_factor_expected,
        const std::unordered_map<core::Identifier, LinearModelParams> &risk_factor_models,
        const Eigen::MatrixXd &risk_factor_cholesky,
        const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
            &rural_prevalence,
        const std::unordered_map<core::Income, LinearModelParams> &income_models);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    void linear_approximation(Person &person);

    Eigen::VectorXd correlated_sample(RuntimeContext &context);

    /// @brief Initialise the sector of a person
    /// @param context The runtime context
    /// @param person The person to initialise sector for
    void initialise_sector(RuntimeContext &context, Person &person) const;

    /// @brief Update the sector of a person
    /// @param context The runtime context
    /// @param person The person to update sector for
    void update_sector(RuntimeContext &context, Person &person) const;

    /// @brief Initialise the income category of a person
    /// @param context The runtime context
    /// @param person The person to initialise sector for
    void initialise_income(RuntimeContext &context, Person &person) const;

    /// @brief Update the income category of a person
    /// @param context The runtime context
    /// @param person The person to update sector for
    void update_income(RuntimeContext &context, Person &person) const;

    const std::unordered_map<core::Identifier, LinearModelParams> &risk_factor_models_;
    const Eigen::MatrixXd &risk_factor_cholesky_;
    const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        &rural_prevalence_;
    const std::unordered_map<core::Income, LinearModelParams> &income_models_;
};

/// @brief Defines the static linear model data type
class StaticLinearModelDefinition : public RiskFactorAdjustableModelDefinition {
  public:
    /// @brief Initialises a new instance of the StaticLinearModelDefinition class
    /// @param risk_factor_expected The risk factor expected values by sex and age
    /// @param risk_factor_models The linear models used to initialise a person's risk factor values
    /// @param risk_factor_cholesky The Cholesky decomposition of the risk factor correlation matrix
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param income_models The income models for each income category
    /// @throws HgpsException for invalid arguments
    StaticLinearModelDefinition(
        RiskFactorSexAgeTable risk_factor_expected,
        std::unordered_map<core::Identifier, LinearModelParams> risk_factor_models,
        Eigen::MatrixXd risk_factor_cholesky,
        std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
            rural_prevalence,
        std::unordered_map<core::Income, LinearModelParams> income_models);

    /// @brief Construct a new StaticLinearModel from this definition
    /// @return A unique pointer to the new StaticLinearModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::unordered_map<core::Identifier, LinearModelParams> risk_factor_models_;
    Eigen::MatrixXd risk_factor_cholesky_;
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        rural_prevalence_;
    std::unordered_map<core::Income, LinearModelParams> income_models_;
};

} // namespace hgps
