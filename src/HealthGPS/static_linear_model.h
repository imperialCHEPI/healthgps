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
/// @details The static model is used to initialise the virtual population.
class StaticLinearModel final : public RiskFactorAdjustableModel {
  public:
    /// @brief Initialises a new instance of the StaticLinearModel class
    /// @param expected The expected risk factor values by sex and age
    /// @param expected_trend The expected trend of risk factor values
    /// @param trend_steps The number of time steps to apply the trend
    /// @param expected_trend_boxcox The expected boxcox factor
    /// @param names The risk factor names
    /// @param models The linear models used to compute a person's risk factor values
    /// @param ranges The value range of each risk factor
    /// @param lambda The lambda values of the risk factors
    /// @param stddev The standard deviations of the risk factors
    /// @param cholesky Cholesky decomposition of the risk factor correlation matrix
    /// @param policy_models The linear models used to compute a person's intervention policies
    /// @param policy_ranges The value range of each intervention policy
    /// @param policy_cholesky Cholesky decomposition of the intervention policy covariance matrix
    /// @param trend_models The linear models used to compute a person's risk factor trends
    /// @param trend_ranges The value range of each risk factor trend
    /// @param trend_lambda The lambda values of the risk factor trends
    /// @param info_speed The information speed of risk factor updates
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param income_models The income models for each income category
    /// @param physical_activity_stddev The standard deviation of the physical activity
    /// @param trend_type The type of trend to apply (None, Regular, or Income)
    /// @param expected_income_trend The expected income trend of risk factor values
    /// @param expected_income_trend_boxcox The expected income trend boxcox factor
    /// @param income_trend_steps The number of time steps to apply the income trend
    /// @param income_trend_models The linear models used to compute income trends
    /// @param income_trend_ranges The value range of each income trend
    /// @param income_trend_lambda The lambda values of the income trends
    /// @param income_trend_decay_factors The exponential decay factors for income trends
    /// @throws HgpsException for invalid arguments
    StaticLinearModel(
        std::shared_ptr<RiskFactorSexAgeTable> expected,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox,
        const std::vector<core::Identifier> &names, const std::vector<LinearModelParams> &models,
        const std::vector<core::DoubleInterval> &ranges, const std::vector<double> &lambda,
        const std::vector<double> &stddev, const Eigen::MatrixXd &cholesky,
        const std::vector<LinearModelParams> &policy_models,
        const std::vector<core::DoubleInterval> &policy_ranges,
        const Eigen::MatrixXd &policy_cholesky,
        std::shared_ptr<std::vector<LinearModelParams>> trend_models,
        std::shared_ptr<std::vector<core::DoubleInterval>> trend_ranges,
        std::shared_ptr<std::vector<double>> trend_lambda, double info_speed,
        const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
            &rural_prevalence,
        const std::unordered_map<core::Income, LinearModelParams> &income_models,
        double physical_activity_stddev, TrendType trend_type,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend =
            nullptr,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox =
            nullptr,
        std::shared_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps = nullptr,
        std::shared_ptr<std::vector<LinearModelParams>> income_trend_models = nullptr,
        std::shared_ptr<std::vector<core::DoubleInterval>> income_trend_ranges = nullptr,
        std::shared_ptr<std::vector<double>> income_trend_lambda = nullptr,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors =
            nullptr);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    static double inverse_box_cox(double factor, double lambda);

    void initialise_factors(RuntimeContext &context, Person &person, Random &random) const;

    void update_factors(RuntimeContext &context, Person &person, Random &random) const;

    void initialise_trends(RuntimeContext &context, Person &person) const;

    void update_trends(RuntimeContext &context, Person &person) const;

    /// @brief Initialise income trends for a person
    /// @param context The runtime context
    /// @param person The person to initialise income trends for
    void initialise_income_trends(RuntimeContext &context, Person &person) const;

    /// @brief Update income trends for a person
    /// @param context The runtime context
    /// @param person The person to update income trends for
    void update_income_trends(RuntimeContext &context, Person &person) const;

    void initialise_policies(Person &person, Random &random, bool intervene) const;

    void update_policies(Person &person, bool intervene) const;

    void apply_policies(Person &person, bool intervene) const;

    std::vector<double> compute_linear_models(Person &person,
                                              const std::vector<LinearModelParams> &models) const;

    std::vector<double> compute_residuals(Random &random, const Eigen::MatrixXd &cholesky) const;

    /// @brief Initialise the sector of a person
    /// @param person The person to initialise sector for
    /// @param random The random number generator from the runtime context
    void initialise_sector(Person &person, Random &random) const;

    /// @brief Update the sector of a person
    /// @param person The person to update sector for
    /// @param random The random number generator from the runtime context
    void update_sector(Person &person, Random &random) const;

    /// @brief Initialise the income category of a person
    /// @param person The person to initialise income for
    /// @param random The random number generator from the runtime context
    void initialise_income(Person &person, Random &random) const;

    /// @brief Update the income category of a person
    /// @param person The person to update income for
    /// @param random The random number generator from the runtime context
    void update_income(Person &person, Random &random) const;

    /// @brief Initialise the physical activity of a person
    /// @param person The person to initialise PAL for
    /// @param random The random number generator from the runtime context
    void initialise_physical_activity(RuntimeContext &context, Person &person,
                                      Random &random) const;

    // Regular trend member variables
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox_;
    std::shared_ptr<std::vector<LinearModelParams>> trend_models_;
    std::shared_ptr<std::vector<core::DoubleInterval>> trend_ranges_;
    std::shared_ptr<std::vector<double>> trend_lambda_;

    // Income trend member variables
    TrendType trend_type_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox_;
    std::shared_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps_;
    std::shared_ptr<std::vector<LinearModelParams>> income_trend_models_;
    std::shared_ptr<std::vector<core::DoubleInterval>> income_trend_ranges_;
    std::shared_ptr<std::vector<double>> income_trend_lambda_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors_;

    // Common member variables
    const std::vector<core::Identifier> &names_;
    const std::vector<LinearModelParams> &models_;
    const std::vector<core::DoubleInterval> &ranges_;
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

    // ===== MAHIMA: Risk Factor Inspection Feature =====
    /// @brief Risk factor inspection data collector
    mutable std::unique_ptr<std::vector<std::string>> inspection_data_;
    
    /// @brief Risk factor inspection settings
    struct InspectionSettings {
        bool enabled{false};
        core::Identifier target_risk_factor;
        std::optional<int> target_age;
        std::optional<core::Gender> target_gender;
        std::optional<int> target_year;
    };
    InspectionSettings inspection_settings_;
    
    /// @brief Initialize inspection settings (called once)
    void initialize_inspection_settings();
    
    /// @brief Check if inspection should be performed for this person and factor
    /// @param person The person to check
    /// @param factor_name The risk factor name
    /// @param context The runtime context
    /// @return true if inspection should be performed
    bool should_inspect(const Person &person, const core::Identifier &factor_name, 
                       const RuntimeContext &context) const;
    
    /// @brief Record inspection data for a risk factor calculation
    /// @param person The person being inspected
    /// @param factor_name The risk factor name
    /// @param context The runtime context
    /// @param step_name The calculation step name
    /// @param value_assigned The assigned value
    /// @param expected_value The expected value
    /// @param linear_result The linear model result
    /// @param residual The residual value
    /// @param stddev The standard deviation
    /// @param lambda The lambda parameter
    /// @param boxcox_result The Box-Cox transformation result
    /// @param factor_before_clamp The factor before clamping
    /// @param range_lower The range lower bound
    /// @param range_upper The range upper bound
    /// @param final_clamped_factor The final clamped factor
    /// @param random_residual_before_cholesky The random residual before Cholesky
    /// @param residual_after_cholesky The residual after Cholesky
    void record_inspection_data(const Person &person, const core::Identifier &factor_name,
                               const RuntimeContext &context, const std::string &step_name,
                               double value_assigned, double expected_value, double linear_result,
                               double residual, double stddev, double lambda, double boxcox_result,
                               double factor_before_clamp, double range_lower, double range_upper,
                               double final_clamped_factor, double random_residual_before_cholesky,
                               double residual_after_cholesky) const;
    
    /// @brief Write inspection data to CSV file
    /// @param context The runtime context
    void write_inspection_data(const RuntimeContext &context) const;
    
    /// @brief Convert inspection record to CSV line
    /// @param person_id The person ID
    /// @param gender The person's gender
    /// @param age The person's age
    /// @param region The person's sector/region
    /// @param income_continuous The person's income as continuous value
    /// @param income_category The person's income category
    /// @param step_name The calculation step name
    /// @param value_assigned The assigned value
    /// @param expected_value The expected value
    /// @param linear_result The linear model result
    /// @param residual The residual value
    /// @param stddev The standard deviation
    /// @param lambda The lambda parameter
    /// @param boxcox_result The Box-Cox transformation result
    /// @param factor_before_clamp The factor before clamping
    /// @param range_lower The range lower bound
    /// @param range_upper The range upper bound
    /// @param final_clamped_factor The final clamped factor
    /// @param random_residual_before_cholesky The random residual before Cholesky
    /// @param residual_after_cholesky The residual after Cholesky
    /// @return CSV formatted string
    std::string create_inspection_csv_line(std::size_t person_id, core::Gender gender, unsigned int age,
                                          core::Sector region, double income_continuous, core::Income income_category,
                                          const std::string &step_name, double value_assigned, double expected_value,
                                          double linear_result, double residual, double stddev, double lambda,
                                          double boxcox_result, double factor_before_clamp, double range_lower,
                                          double range_upper, double final_clamped_factor,
                                          double random_residual_before_cholesky, double residual_after_cholesky) const;
    
    /// @brief Get CSV header for inspection data
    /// @return CSV header string
    static std::string get_inspection_csv_header();
    // ===== END MAHIMA: Risk Factor Inspection Feature =====
};

/// @brief Defines the static linear model data type
class StaticLinearModelDefinition : public RiskFactorAdjustableModelDefinition {
  public:
    /// @brief Initialises a new instance of the StaticLinearModelDefinition class
    /// @param expected The expected risk factor values by sex and age
    /// @param expected_trend The expected trend of risk factor values
    /// @param trend_steps The number of time steps to apply the trend
    /// @param expected_trend_boxcox The expected boxcox factor
    /// @param names The risk factor names
    /// @param models The linear models used to compute a person's risk factors
    /// @param ranges The value range of each risk factor
    /// @param lambda The lambda values of the risk factors
    /// @param stddev The standard deviations of the risk factors
    /// @param cholesky Cholesky decomposition of the risk factor correlation matrix
    /// @param policy_models The linear models used to compute a person's intervention policies
    /// @param policy_ranges The value range of each intervention policy
    /// @param policy_cholesky Cholesky decomposition of the intervention policy covariance matrix
    /// @param trend_models The linear models used to compute a person's risk factor trends
    /// @param trend_ranges The value range of each risk factor trend
    /// @param trend_lambda The lambda values of the risk factor trends
    /// @param info_speed The information speed of risk factor updates
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param income_models The income models for each income category
    /// @param physical_activity_stddev The standard deviation of the physical activity
    /// @param trend_type The type of trend to apply (None, Regular, or Income)
    /// @param expected_income_trend The expected income trend of risk factor values
    /// @param expected_income_trend_boxcox The expected income trend boxcox factor
    /// @param income_trend_steps The number of time steps to apply the income trend
    /// @param income_trend_models The linear models used to compute income trends
    /// @param income_trend_ranges The value range of each income trend
    /// @param income_trend_lambda The lambda values of the income trends
    /// @param income_trend_decay_factors The exponential decay factors for income trends
    /// @throws HgpsException for invalid arguments
    StaticLinearModelDefinition(
        std::unique_ptr<RiskFactorSexAgeTable> expected,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox,
        std::vector<core::Identifier> names, std::vector<LinearModelParams> models,
        std::vector<core::DoubleInterval> ranges, std::vector<double> lambda,
        std::vector<double> stddev, Eigen::MatrixXd cholesky,
        std::vector<LinearModelParams> policy_models,
        std::vector<core::DoubleInterval> policy_ranges, Eigen::MatrixXd policy_cholesky,
        std::unique_ptr<std::vector<LinearModelParams>> trend_models,
        std::unique_ptr<std::vector<core::DoubleInterval>> trend_ranges,
        std::unique_ptr<std::vector<double>> trend_lambda, double info_speed,
        std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
            rural_prevalence,
        std::unordered_map<core::Income, LinearModelParams> income_models,
        double physical_activity_stddev, TrendType trend_type = TrendType::Null,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend =
            nullptr,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox =
            nullptr,
        std::unique_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps = nullptr,
        std::unique_ptr<std::vector<LinearModelParams>> income_trend_models = nullptr,
        std::unique_ptr<std::vector<core::DoubleInterval>> income_trend_ranges = nullptr,
        std::unique_ptr<std::vector<double>> income_trend_lambda = nullptr,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors =
            nullptr);

    /// @brief Construct a new StaticLinearModel from this definition
    /// @return A unique pointer to the new StaticLinearModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    // Regular trend member variables
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox_;
    std::shared_ptr<std::vector<LinearModelParams>> trend_models_;
    std::shared_ptr<std::vector<core::DoubleInterval>> trend_ranges_;
    std::shared_ptr<std::vector<double>> trend_lambda_;

    // Income trend member variables
    TrendType trend_type_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox_;
    std::shared_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps_;
    std::shared_ptr<std::vector<LinearModelParams>> income_trend_models_;
    std::shared_ptr<std::vector<core::DoubleInterval>> income_trend_ranges_;
    std::shared_ptr<std::vector<double>> income_trend_lambda_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors_;

    // Common member variables
    std::vector<core::Identifier> names_;
    std::vector<LinearModelParams> models_;
    std::vector<core::DoubleInterval> ranges_;
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
