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

/// @brief Defines the physical activity model parameters
/// @details Supports both simple (India) and continuous (FINCH) approaches
struct PhysicalActivityModel {
    /// @brief Model type: "simple" for India approach, "continuous" for FINCH approach
    std::string model_type{"simple"};

    /// @brief Intercept value for the linear model
    double intercept{};

    /// @brief Coefficients for each factor (age, gender, region, ethnicity, etc.)
    std::unordered_map<core::Identifier, double> coefficients{};

    /// @brief Minimum allowed value for physical activity
    double min_value = 0.0;

    /// @brief Maximum allowed value for physical activity
    double max_value = std::numeric_limits<double>::max();

    /// @brief Standard deviation for simple models (India approach)
    double stddev = 0.06;
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
    /// @param is_continuous_income_model Whether this model uses continuous income calculation
    /// (FINCH approach)
    /// @param continuous_income_model The continuous income model parameters (if using FINCH
    /// approach)
    /// @param income_categories The number of income categories (3 or 4)
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
            nullptr,
        bool is_continuous_income_model = false,
        const LinearModelParams &continuous_income_model = LinearModelParams{},
        const std::string &income_categories = "3",
        /// @param physical_activity_models Physical activity models for both India (simple) and
        /// FINCH (continuous) approaches
        const std::unordered_map<core::Identifier, PhysicalActivityModel>
            &physical_activity_models = {},
        bool has_active_policies = true);

        RiskFactorModelType type() const noexcept override;

        std::string name() const noexcept override;

        void generate_risk_factors(RuntimeContext &context) override;

        void update_risk_factors(RuntimeContext &context) override;

      private:
        static double inverse_box_cox(double factor, double lambda);

        void initialise_factors(RuntimeContext &context, Person &person, Random &random) const;

        void update_factors(RuntimeContext &context, Person &person, Random &random) const;

        void initialise_UPF_trends(RuntimeContext &context, Person &person) const;

        void update_UPF_trends(RuntimeContext &context, Person &person) const;

        /// @brief Initialise income for a person using the appropriate method
        /// @param context The runtime context
        /// @param person The person to initialise income for
        /// @param random Random number generator
        void initialise_income(RuntimeContext &context, Person &person, Random &random);

        /// @brief Update income for a person (only for 18-year-olds)
        /// @param context The runtime context
        /// @param person The person to update income for
        /// @param random Random number generator
        void update_income(RuntimeContext &context, Person &person, Random &random);

        /// @brief Update income trends for a person
        /// @param context The runtime context
        /// @param person The person to update income trends for
        void update_income_trends(RuntimeContext &context, Person &person) const;

        /// @brief Initialise income trends for a person
        /// @param context The runtime context
        /// @param person The person to initialise income trends for
        void initialise_income_trends(RuntimeContext &context, Person &person) const;

        void initialise_policies(Person &person, Random &random, bool intervene) const;

        void update_policies(Person &person, bool intervene) const;

        void apply_policies(Person &person, bool intervene) const;

        std::vector<double>
        compute_linear_models(Person &person, const std::vector<LinearModelParams> &models) const;

        std::vector<double> compute_residuals(Random &random,
                                              const Eigen::MatrixXd &cholesky) const;

        /// @brief Initialise the sector of a person
        /// @param person The person to initialise sector for
        /// @param random The random number generator from the runtime context
        void initialise_sector(Person &person, Random &random) const;

        /// @brief Update the sector of a person
        /// @param person The person to update sector for
        /// @param random The random number generator from the runtime context
        void update_sector(Person &person, Random &random) const;

        // Continuous income model support (FINCH approach)
        bool is_continuous_income_model_;
        LinearModelParams continuous_income_model_;
        std::string income_categories_;

        /// @brief Calculate continuous income using FINCH approach
        /// @param person The person to calculate income for
        /// @param random Random number generator
        /// @return The calculated continuous income value
        double calculate_continuous_income(Person &person, Random &random);

        /// @brief Convert continuous income to income category based on population quartiles
        /// @param continuous_income The continuous income value
        /// @param population The population to calculate quartiles from
        /// @param random Random number generator
        /// @return The assigned income category
        core::Income convert_income_continuous_to_category(double continuous_income,
                                                           const Population &population,
                                                           Random &random) const;

        /// @brief Calculate income quartiles from population data
        /// @param population The population to calculate quartiles from
        /// @return Vector of quartile thresholds [Q1, Q2, Q3]
        std::vector<double> calculate_income_quartiles(const Population &population) const;

        /// @brief Initialise income using categorical approach (India method)
        /// @param person The person to initialise income for
        /// @param random Random number generator
        void initialise_categorical_income(Person &person, Random &random);

        /// @brief Initialise income using continuous approach (FINCH method)
        /// @param context The runtime context
        /// @param person The person to initialise income for
        /// @param random Random number generator
        void initialise_continuous_income(RuntimeContext &context, Person &person, Random &random);

        /// @brief Initialise the physical activity of a person
        /// @param person The person to initialise PAL for
        /// @param random The random number generator from the runtime context
        void initialise_physical_activity(RuntimeContext &context, Person &person,
                                          Random &random) const;

        /// @brief Initialise physical activity using continuous model approach (FINCH method)
        /// @param context The runtime context
        /// @param person The person to initialise physical activity for
        /// @param random Random number generator
        /// @param model The physical activity model to use
        void initialise_continuous_physical_activity(RuntimeContext &context, Person &person,
                                                     Random &random,
                                                     const PhysicalActivityModel &model) const;

        /// @brief Initialise physical activity using simple model approach (India method)
        /// @param context The runtime context
        /// @param person The person to initialise physical activity for
        /// @param random Random number generator
        /// @param model The physical activity model to use
        void initialise_simple_physical_activity(RuntimeContext &context, Person &person,
                                                 Random &random,
                                                 const PhysicalActivityModel &model) const;

        // Regular trend member variables
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
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

        // Physical activity model support (both India and FINCH approaches)
        std::unordered_map<core::Identifier, PhysicalActivityModel> physical_activity_models_;
        bool has_physical_activity_models_ = false;
        // Policy optimization flag - Mahima's enhancement
        bool has_active_policies_;
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
    /// @param is_continuous_income_model Whether this model uses continuous income calculation
    /// (FINCH approach)
    /// @param continuous_income_model The continuous income model parameters (if using FINCH
    /// approach)
    /// @param income_categories The number of income categories (3 or 4)
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
            nullptr,
        bool is_continuous_income_model = false,
        const LinearModelParams &continuous_income_model = LinearModelParams{},
        const std::string &income_categories = "3",
        std::unordered_map<core::Identifier, PhysicalActivityModel> physical_activity_models = {},
        bool has_active_policies = true);

        /// @brief Construct a new StaticLinearModel from this definition
        /// @return A unique pointer to the new StaticLinearModel instance
        std::unique_ptr<RiskFactorModel> create_model() const override;

      private:
        // Regular trend member variables
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
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

        // Physical activity model support (FINCH approach)
        std::unordered_map<core::Identifier, PhysicalActivityModel> physical_activity_models_;
        bool has_physical_activity_models_ = false;

        // Continuous income model support (FINCH approach)
        bool is_continuous_income_model_;
        LinearModelParams continuous_income_model_;
        std::string income_categories_;
        // Policy optimization flag - Mahima's enhancement
        bool has_active_policies_;
};

} // namespace hgps
