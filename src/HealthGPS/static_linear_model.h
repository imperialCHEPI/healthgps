#pragma once

#include "interfaces.h"
#include "mapping.h"
#include "risk_factor_adjustable_model.h"

#include <Eigen/Dense>
#include <vector>

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
    /// @param expected The risk factor expected values by sex and age
    /// @param names The risk factor names
    /// @param models The linear models used to compute a person's risk factor values
    /// @param lambda The lambda values of the risk factors
    /// @param stddev The standard deviations of the risk factors
    /// @param cholesky The Cholesky decomposition of the risk factor correlation matrix
    /// @param info_speed The information speed of risk factor updates
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param income_models The income models for each income category
    /// @throws HgpsException for invalid arguments
    StaticLinearModel(
        const RiskFactorSexAgeTable &expected, const std::vector<core::Identifier> &names,
        const std::vector<LinearModelParams> &models, const std::vector<double> &lambda,
        const std::vector<double> &stddev, const Eigen::MatrixXd &cholesky, const double info_speed,
        const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
            &rural_prevalence,
        const std::unordered_map<core::Income, LinearModelParams> &income_models);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    void compute_linear_models(Person &person);

    void compute_correlated_residuals(Person &person, Random &random);

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

    const std::vector<core::Identifier> &names_;
    const std::vector<LinearModelParams> &models_;
    const std::vector<double> &lambda_;
    const std::vector<double> &stddev_;
    const Eigen::MatrixXd &cholesky_;
    const double info_speed_;
    const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        &rural_prevalence_;
    const std::unordered_map<core::Income, LinearModelParams> &income_models_;
};

/// @brief Defines the static linear model data type
class StaticLinearModelDefinition : public RiskFactorAdjustableModelDefinition {
  public:
    /// @brief Initialises a new instance of the StaticLinearModelDefinition class
    /// @param expected The risk factor expected values by sex and age
    /// @param names The risk factor names
    /// @param models The linear models used to compute a person's risk factor values
    /// @param lambda The lambda values of the risk factors
    /// @param stddev The standard deviations of the risk factors
    /// @param cholesky The Cholesky decomposition of the risk factor correlation matrix
    /// @param info_speed The information speed of risk factor updates
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param income_models The income models for each income category
    /// @throws HgpsException for invalid arguments
    StaticLinearModelDefinition(
        RiskFactorSexAgeTable expected, std::vector<core::Identifier> names,
        std::vector<LinearModelParams> models, std::vector<double> lambda,
        std::vector<double> stddev, Eigen::MatrixXd cholesky, double info_speed,
        std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
            rural_prevalence,
        std::unordered_map<core::Income, LinearModelParams> income_models);

    /// @brief Construct a new StaticLinearModel from this definition
    /// @return A unique pointer to the new StaticLinearModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::vector<core::Identifier> names_;
    std::vector<LinearModelParams> models_;
    std::vector<double> lambda_;
    std::vector<double> stddev_;
    Eigen::MatrixXd cholesky_;
    double info_speed_;
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        rural_prevalence_;
    std::unordered_map<core::Income, LinearModelParams> income_models_;
};

} // namespace hgps
