#pragma once

#include "interfaces.h"
#include "mapping.h"
#include "risk_factor_adjustable_model.h"

#include <Eigen/Dense>
#include <unordered_map>
#include <vector>

namespace hgps {

/// @brief Defines the linear model parameters used to initialise risk factors
struct LinearModelParams {
    double intercept{};
    std::unordered_map<core::Identifier, double> coefficients{};
    std::unordered_map<core::Identifier, double> log_coefficients{};
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
    /// @param cholesky Cholesky decomposition of the risk factor correlation matrix
    /// @param policy_models The linear models used to compute a person's intervention policies
    /// @param policy_ranges The boundaries of the intervention policy values
    /// @param policy_cholesky Cholesky decomposition of the intervention policy covariance matrix
    /// @param info_speed The information speed of risk factor updates
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param income_models The income models for each income category
    /// @param phycical_activity_stddev The standard deviation of the physical activity
    /// @throws HgpsException for invalid arguments
    StaticLinearModel(
        const RiskFactorSexAgeTable &expected, const std::vector<core::Identifier> &names,
        const std::vector<LinearModelParams> &models, const std::vector<double> &lambda,
        const std::vector<double> &stddev, const Eigen::MatrixXd &cholesky,
        const std::vector<LinearModelParams> &policy_models,
        const std::vector<core::DoubleInterval> &policy_ranges,
        const Eigen::MatrixXd &policy_cholesky, double info_speed,
        const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
            &rural_prevalence,
        const std::unordered_map<core::Income, LinearModelParams> &income_models,
        double physical_activity_stddev);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    void initialise_factors(Person &person, Random &random) const;

    void update_factors(Person &person, Random &random) const;

    std::vector<double> compute_linear_models(Person &person) const;

    std::vector<double> compute_residuals(Random &random) const;

    static double inverse_box_cox(double factor, double lambda);

    /// @brief Initialise the sector of a person
    /// @param person The person to initialise sector for
    /// @param random The random number generator from the runtime context
    void initialise_sector(Person &person, Random &random) const;

    /// @brief Update the sector of a person
    /// @param person The person to update sector for
    /// @param random The random number generator from the runtime context
    void update_sector(Person &person, Random &random) const;

    /// @brief Initialise the income category of a person
    /// @param person The person to initialise sector for
    /// @param random The random number generator from the runtime context
    void initialise_income(Person &person, Random &random) const;

    /// @brief Update the income category of a person
    /// @param person The person to update sector for
    /// @param random The random number generator from the runtime context
    void update_income(Person &person, Random &random) const;

    /// @brief Initialise the physical activity of a person
    /// @param person The person to initialise sector for
    /// @param random The random number generator from the runtime context
    void initialise_physical_activity(Person &person, Random &random) const;

    const std::vector<core::Identifier> &names_;
    const std::vector<LinearModelParams> &models_;
    const std::vector<double> &lambda_;
    const std::vector<double> &stddev_;
    const Eigen::MatrixXd &cholesky_;
    const std::vector<LinearModelParams> &policy_models_;
    const std::vector<core::DoubleInterval> &policy_ranges_;
    const Eigen::MatrixXd &policy_cholesky_;
    const double info_speed_;
    const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        &rural_prevalence_;
    const std::unordered_map<core::Income, LinearModelParams> &income_models_;
    const double physical_activity_stddev_;
};

/// @brief Defines the static linear model data type
class StaticLinearModelDefinition : public RiskFactorAdjustableModelDefinition {
  public:
    /// @brief Initialises a new instance of the StaticLinearModelDefinition class
    /// @param expected The risk factor expected values by sex and age
    /// @param names The risk factor names
    /// @param models The linear models used to compute a person's risk factors
    /// @param lambda The lambda values of the risk factors
    /// @param stddev The standard deviations of the risk factors
    /// @param cholesky Cholesky decomposition of the risk factor correlation matrix
    /// @param policy_models The linear models used to compute a person's intervention policies
    /// @param policy_ranges The boundaries of the intervention policy values
    /// @param policy_cholesky Cholesky decomposition of the intervention policy covariance matrix
    /// @param info_speed The information speed of risk factor updates
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param income_models The income models for each income category
    /// @param phycical_activity_stddev The standard deviation of the physical activity
    /// @throws HgpsException for invalid arguments
    StaticLinearModelDefinition(
        RiskFactorSexAgeTable expected, std::vector<core::Identifier> names,
        std::vector<LinearModelParams> models, std::vector<double> lambda,
        std::vector<double> stddev, Eigen::MatrixXd cholesky,
        std::vector<LinearModelParams> policy_models,
        std::vector<core::DoubleInterval> policy_ranges, Eigen::MatrixXd policy_cholesky,
        double info_speed,
        std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
            rural_prevalence,
        std::unordered_map<core::Income, LinearModelParams> income_models,
        double physical_activity_stddev);

    /// @brief Construct a new StaticLinearModel from this definition
    /// @return A unique pointer to the new StaticLinearModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::vector<core::Identifier> names_;
    std::vector<LinearModelParams> models_;
    std::vector<double> lambda_;
    std::vector<double> stddev_;
    Eigen::MatrixXd cholesky_;
    std::vector<LinearModelParams> policy_models_;
    std::vector<core::DoubleInterval> policy_ranges_;
    Eigen::MatrixXd policy_cholesky_;
    double info_speed_;
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        rural_prevalence_;
    std::unordered_map<core::Income, LinearModelParams> income_models_;
    double physical_activity_stddev_;
};

} // namespace hgps
