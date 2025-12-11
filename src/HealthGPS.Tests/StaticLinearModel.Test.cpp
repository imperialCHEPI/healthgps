#include "pch.h"

#include "HealthGPS/static_linear_model.h"
#include "HealthGPS/runtime_context.h"
#include "HealthGPS/person.h"
#include "HealthGPS/random_algorithm.h"
#include "HealthGPS/gender_table.h"
#include "HealthGPS.Core/identifier.h"
#include "HealthGPS.Core/interval.h"

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <Eigen/Dense>

using namespace hgps;
using namespace hgps::core;

class StaticLinearModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test data
        create_test_expected_values();
        create_test_risk_factor_data();
        create_test_policy_data();
        create_test_trend_data();
        create_test_income_trend_data();
        create_test_rural_prevalence();
        create_test_income_models();
        create_test_cholesky_matrices();
    }

    void create_test_expected_values() {
        expected_ = std::make_shared<RiskFactorSexAgeTable>();
        
        // Create age range 0-100
        auto age_range = IntegerInterval(0, 100);
        expected_->emplace_row(Gender::male, create_age_gender_table<double>(age_range));
        expected_->emplace_row(Gender::female, create_age_gender_table<double>(age_range));
        
        // Set some test values
        for (int age = 0; age <= 100; age++) {
            expected_->at(Gender::male)[age] = 25.0 + age * 0.1;
            expected_->at(Gender::female)[age] = 23.0 + age * 0.1;
        }
    }

    void create_test_risk_factor_data() {
        names_ = {"BMI"_id, "SBP"_id};
        
        // Create linear models
        LinearModelParams bmi_model;
        bmi_model.intercept = 20.0;
        bmi_model.coefficients["Age"_id] = 0.1;
        bmi_model.coefficients["SES"_id] = -0.5;
        
        LinearModelParams sbp_model;
        sbp_model.intercept = 120.0;
        sbp_model.coefficients["Age"_id] = 0.8;
        sbp_model.coefficients["BMI"_id] = 0.5;
        
        models_ = {bmi_model, sbp_model};
        
        // Create ranges
        ranges_ = {
            DoubleInterval(15.0, 50.0),  // BMI range
            DoubleInterval(80.0, 200.0)  // SBP range
        };
        
        // Create lambda values
        lambda_ = {0.5, 1.0};
        
        // Create standard deviations
        stddev_ = {2.0, 10.0};
    }

    void create_test_policy_data() {
        // Create policy models
        LinearModelParams bmi_policy;
        bmi_policy.intercept = 0.0;
        bmi_policy.coefficients["Age"_id] = 0.01;
        
        LinearModelParams sbp_policy;
        sbp_policy.intercept = 0.0;
        sbp_policy.coefficients["BMI"_id] = 0.02;
        
        policy_models_ = {bmi_policy, sbp_policy};
        
        // Create policy ranges
        policy_ranges_ = {
            DoubleInterval(-10.0, 10.0),  // BMI policy range
            DoubleInterval(-20.0, 20.0)  // SBP policy range
        };
    }

    void create_test_trend_data() {
        expected_trend_ = std::make_shared<std::unordered_map<Identifier, double>>();
        expected_trend_->emplace("BMI"_id, 1.02);
        expected_trend_->emplace("SBP"_id, 1.01);
        
        trend_steps_ = std::make_shared<std::unordered_map<Identifier, int>>();
        trend_steps_->emplace("BMI"_id, 10);
        trend_steps_->emplace("SBP"_id, 15);
        
        expected_trend_boxcox_ = std::make_shared<std::unordered_map<Identifier, double>>();
        expected_trend_boxcox_->emplace("BMI"_id, 1.02);
        expected_trend_boxcox_->emplace("SBP"_id, 1.01);
        
        // Create trend models
        LinearModelParams bmi_trend;
        bmi_trend.intercept = 0.02;
        bmi_trend.coefficients["Age"_id] = 0.001;
        
        LinearModelParams sbp_trend;
        sbp_trend.intercept = 0.01;
        sbp_trend.coefficients["Age"_id] = 0.0005;
        
        trend_models_ = std::make_shared<std::vector<LinearModelParams>>();
        trend_models_->push_back(bmi_trend);
        trend_models_->push_back(sbp_trend);
        
        trend_ranges_ = std::make_shared<std::vector<DoubleInterval>>();
        trend_ranges_->push_back(DoubleInterval(0.8, 1.2));
        trend_ranges_->push_back(DoubleInterval(0.9, 1.1));
        
        trend_lambda_ = std::make_shared<std::vector<double>>();
        trend_lambda_->push_back(0.5);
        trend_lambda_->push_back(1.0);
    }

    void create_test_income_trend_data() {
        expected_income_trend_ = std::make_shared<std::unordered_map<Identifier, double>>();
        expected_income_trend_->emplace("BMI"_id, 1.05);
        expected_income_trend_->emplace("SBP"_id, 1.03);
        
        expected_income_trend_boxcox_ = std::make_shared<std::unordered_map<Identifier, double>>();
        expected_income_trend_boxcox_->emplace("BMI"_id, 1.05);
        expected_income_trend_boxcox_->emplace("SBP"_id, 1.03);
        
        income_trend_steps_ = std::make_shared<std::unordered_map<Identifier, int>>();
        income_trend_steps_->emplace("BMI"_id, 20);
        income_trend_steps_->emplace("SBP"_id, 25);
        
        income_trend_decay_factors_ = std::make_shared<std::unordered_map<Identifier, double>>();
        income_trend_decay_factors_->emplace("BMI"_id, 0.02);
        income_trend_decay_factors_->emplace("SBP"_id, 0.015);
        
        // Create income trend models
        LinearModelParams bmi_income_trend;
        bmi_income_trend.intercept = 0.05;
        bmi_income_trend.coefficients["Income"_id] = 0.01;
        
        LinearModelParams sbp_income_trend;
        sbp_income_trend.intercept = 0.03;
        sbp_income_trend.coefficients["Income"_id] = 0.005;
        
        income_trend_models_ = std::make_shared<std::vector<LinearModelParams>>();
        income_trend_models_->push_back(bmi_income_trend);
        income_trend_models_->push_back(sbp_income_trend);
        
        income_trend_ranges_ = std::make_shared<std::vector<DoubleInterval>>();
        income_trend_ranges_->push_back(DoubleInterval(0.7, 1.3));
        income_trend_ranges_->push_back(DoubleInterval(0.8, 1.2));
        
        income_trend_lambda_ = std::make_shared<std::vector<double>>();
        income_trend_lambda_->push_back(0.3);
        income_trend_lambda_->push_back(0.8);
    }

    void create_test_rural_prevalence() {
        rural_prevalence_["Under18"_id] = {{Gender::male, 0.3}, {Gender::female, 0.25}};
        rural_prevalence_["Over18"_id] = {{Gender::male, 0.2}, {Gender::female, 0.15}};
    }

    void create_test_income_models() {
        LinearModelParams low_income;
        low_income.intercept = 1.0;
        low_income.coefficients["SES"_id] = -0.5;
        
        LinearModelParams middle_income;
        middle_income.intercept = 0.0;
        middle_income.coefficients["SES"_id] = 0.0;
        
        LinearModelParams high_income;
        high_income.intercept = -1.0;
        high_income.coefficients["SES"_id] = 0.5;
        
        income_models_[Income::low] = low_income;
        income_models_[Income::middle] = middle_income;
        income_models_[Income::high] = high_income;
    }

    void create_test_cholesky_matrices() {
        // Create 2x2 correlation matrix
        Eigen::MatrixXd correlation(2, 2);
        correlation << 1.0, 0.3,
                      0.3, 1.0;
        
        cholesky_ = Eigen::LLT<Eigen::MatrixXd>(correlation).matrixL();
        
        // Create policy covariance matrix
        Eigen::MatrixXd policy_covariance(2, 2);
        policy_covariance << 1.0, 0.2,
                            0.2, 1.0;
        
        policy_cholesky_ = Eigen::LLT<Eigen::MatrixXd>(policy_covariance).matrixL();
    }

    // Test data members
    std::shared_ptr<RiskFactorSexAgeTable> expected_;
    std::shared_ptr<std::unordered_map<Identifier, double>> expected_trend_;
    std::shared_ptr<std::unordered_map<Identifier, int>> trend_steps_;
    std::shared_ptr<std::unordered_map<Identifier, double>> expected_trend_boxcox_;
    std::vector<Identifier> names_;
    std::vector<LinearModelParams> models_;
    std::vector<DoubleInterval> ranges_;
    std::vector<double> lambda_;
    std::vector<double> stddev_;
    Eigen::MatrixXd cholesky_;
    std::vector<LinearModelParams> policy_models_;
    std::vector<DoubleInterval> policy_ranges_;
    Eigen::MatrixXd policy_cholesky_;
    std::shared_ptr<std::vector<LinearModelParams>> trend_models_;
    std::shared_ptr<std::vector<DoubleInterval>> trend_ranges_;
    std::shared_ptr<std::vector<double>> trend_lambda_;
    std::unordered_map<Identifier, std::unordered_map<Gender, double>> rural_prevalence_;
    std::unordered_map<Income, LinearModelParams> income_models_;
    double physical_activity_stddev_ = 0.1;
    double info_speed_ = 0.5;
    
    // Income trend data
    std::shared_ptr<std::unordered_map<Identifier, double>> expected_income_trend_;
    std::shared_ptr<std::unordered_map<Identifier, double>> expected_income_trend_boxcox_;
    std::shared_ptr<std::unordered_map<Identifier, int>> income_trend_steps_;
    std::shared_ptr<std::vector<LinearModelParams>> income_trend_models_;
    std::shared_ptr<std::vector<DoubleInterval>> income_trend_ranges_;
    std::shared_ptr<std::vector<double>> income_trend_lambda_;
    std::shared_ptr<std::unordered_map<Identifier, double>> income_trend_decay_factors_;
};

TEST_F(StaticLinearModelTest, ConstructorWithNullTrend) {
    auto model = std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_,
        names_, models_, ranges_, lambda_, stddev_, cholesky_,
        policy_models_, policy_ranges_, policy_cholesky_,
        nullptr, nullptr, nullptr,  // No trend models
        info_speed_, rural_prevalence_, income_models_, physical_activity_stddev_,
        TrendType::Null, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        true);
    
    EXPECT_EQ(RiskFactorModelType::Static, model->type());
    EXPECT_EQ("Static", model->name());
}

TEST_F(StaticLinearModelTest, ConstructorWithRegularTrend) {
    auto model = std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_,
        names_, models_, ranges_, lambda_, stddev_, cholesky_,
        policy_models_, policy_ranges_, policy_cholesky_,
        trend_models_, trend_ranges_, trend_lambda_,
        info_speed_, rural_prevalence_, income_models_, physical_activity_stddev_,
        TrendType::Trend, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        true);
    
    EXPECT_EQ(RiskFactorModelType::Static, model->type());
    EXPECT_EQ("Static", model->name());
}

TEST_F(StaticLinearModelTest, ConstructorWithIncomeTrend) {
    auto model = std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_,
        names_, models_, ranges_, lambda_, stddev_, cholesky_,
        policy_models_, policy_ranges_, policy_cholesky_,
        trend_models_, trend_ranges_, trend_lambda_,
        info_speed_, rural_prevalence_, income_models_, physical_activity_stddev_,
        TrendType::IncomeTrend, expected_income_trend_, expected_income_trend_boxcox_,
        income_trend_steps_, income_trend_models_, income_trend_ranges_,
        income_trend_lambda_, income_trend_decay_factors_, true);
    
    EXPECT_EQ(RiskFactorModelType::Static, model->type());
    EXPECT_EQ("Static", model->name());
}

TEST_F(StaticLinearModelTest, ConstructorWithNoActivePolicies) {
    auto model = std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_,
        names_, models_, ranges_, lambda_, stddev_, cholesky_,
        policy_models_, policy_ranges_, policy_cholesky_,
        nullptr, nullptr, nullptr,
        info_speed_, rural_prevalence_, income_models_, physical_activity_stddev_,
        TrendType::Null, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        false);  // No active policies
    
    EXPECT_EQ(RiskFactorModelType::Static, model->type());
    EXPECT_EQ("Static", model->name());
}

TEST_F(StaticLinearModelTest, InverseBoxCoxTransform) {
    // Test inverse Box-Cox transformation
    double factor = 0.5;
    double lambda = 0.5;
    
    double result = StaticLinearModel::inverse_box_cox(factor, lambda);
    
    // Expected: (0.5 * 0.5 + 1.0)^(1/0.5) = (1.25)^2 = 1.5625
    EXPECT_NEAR(1.5625, result, 1e-6);
}

TEST_F(StaticLinearModelTest, InverseBoxCoxTransformWithLambdaOne) {
    double factor = 0.5;
    double lambda = 1.0;
    
    double result = StaticLinearModel::inverse_box_cox(factor, lambda);
    
    // Expected: (0.5 * 1.0 + 1.0)^(1/1.0) = 1.5
    EXPECT_NEAR(1.5, result, 1e-6);
}

TEST_F(StaticLinearModelTest, InverseBoxCoxTransformWithZeroLambda) {
    double factor = 0.5;
    double lambda = 0.0;
    
    // This should handle the special case where lambda approaches 0
    double result = StaticLinearModel::inverse_box_cox(factor, lambda);
    
    // Should not crash and return a reasonable value
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(StaticLinearModelTest, LinearModelParamsStructure) {
    LinearModelParams params;
    params.intercept = 10.0;
    params.coefficients["Age"_id] = 0.5;
    params.coefficients["BMI"_id] = 0.2;
    params.log_coefficients["Income"_id] = 0.1;
    
    EXPECT_EQ(10.0, params.intercept);
    EXPECT_EQ(2, params.coefficients.size());
    EXPECT_EQ(1, params.log_coefficients.size());
    EXPECT_EQ(0.5, params.coefficients.at("Age"_id));
    EXPECT_EQ(0.2, params.coefficients.at("BMI"_id));
    EXPECT_EQ(0.1, params.log_coefficients.at("Income"_id));
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionConstructor) {
    auto definition = std::make_unique<StaticLinearModelDefinition>(
        std::move(expected_), std::move(expected_trend_), std::move(trend_steps_),
        std::move(expected_trend_boxcox_), std::move(names_), std::move(models_),
        std::move(ranges_), std::move(lambda_), std::move(stddev_), std::move(cholesky_),
        std::move(policy_models_), std::move(policy_ranges_), std::move(policy_cholesky_),
        std::move(trend_models_), std::move(trend_ranges_), std::move(trend_lambda_),
        info_speed_, std::move(rural_prevalence_), std::move(income_models_),
        physical_activity_stddev_, TrendType::Null, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, true);
    
    auto model = definition->create_model();
    EXPECT_NE(nullptr, model);
    EXPECT_EQ(RiskFactorModelType::Static, model->type());
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionWithTrendType) {
    auto definition = std::make_unique<StaticLinearModelDefinition>(
        std::move(expected_), std::move(expected_trend_), std::move(trend_steps_),
        std::move(expected_trend_boxcox_), std::move(names_), std::move(models_),
        std::move(ranges_), std::move(lambda_), std::move(stddev_), std::move(cholesky_),
        std::move(policy_models_), std::move(policy_ranges_), std::move(policy_cholesky_),
        std::move(trend_models_), std::move(trend_ranges_), std::move(trend_lambda_),
        info_speed_, std::move(rural_prevalence_), std::move(income_models_),
        physical_activity_stddev_, TrendType::Trend, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, true);
    
    auto model = definition->create_model();
    EXPECT_NE(nullptr, model);
    EXPECT_EQ(RiskFactorModelType::Static, model->type());
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionWithIncomeTrend) {
    auto definition = std::make_unique<StaticLinearModelDefinition>(
        std::move(expected_), std::move(expected_trend_), std::move(trend_steps_),
        std::move(expected_trend_boxcox_), std::move(names_), std::move(models_),
        std::move(ranges_), std::move(lambda_), std::move(stddev_), std::move(cholesky_),
        std::move(policy_models_), std::move(policy_ranges_), std::move(policy_cholesky_),
        std::move(trend_models_), std::move(trend_ranges_), std::move(trend_lambda_),
        info_speed_, std::move(rural_prevalence_), std::move(income_models_),
        physical_activity_stddev_, TrendType::IncomeTrend, std::move(expected_income_trend_),
        std::move(expected_income_trend_boxcox_), std::move(income_trend_steps_),
        std::move(income_trend_models_), std::move(income_trend_ranges_),
        std::move(income_trend_lambda_), std::move(income_trend_decay_factors_), true);
    
    auto model = definition->create_model();
    EXPECT_NE(nullptr, model);
    EXPECT_EQ(RiskFactorModelType::Static, model->type());
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionValidation) {
    // Test with empty names - should throw
    std::vector<Identifier> empty_names;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(), 
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            empty_names, models_, ranges_, lambda_, stddev_, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, rural_prevalence_,
            income_models_, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionEmptyModels) {
    std::vector<LinearModelParams> empty_models;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, empty_models, ranges_, lambda_, stddev_, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, rural_prevalence_,
            income_models_, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionEmptyRanges) {
    std::vector<DoubleInterval> empty_ranges;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, empty_ranges, lambda_, stddev_, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, rural_prevalence_,
            income_models_, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionEmptyLambda) {
    std::vector<double> empty_lambda;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, ranges_, empty_lambda, stddev_, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, rural_prevalence_,
            income_models_, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionEmptyStdDev) {
    std::vector<double> empty_stddev;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, ranges_, lambda_, empty_stddev, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, rural_prevalence_,
            income_models_, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionEmptyPolicyModels) {
    std::vector<LinearModelParams> empty_policy_models;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, ranges_, lambda_, stddev_, cholesky_,
            empty_policy_models, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, rural_prevalence_,
            income_models_, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionEmptyPolicyRanges) {
    std::vector<DoubleInterval> empty_policy_ranges;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, ranges_, lambda_, stddev_, cholesky_,
            policy_models_, empty_policy_ranges, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, rural_prevalence_,
            income_models_, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionEmptyRuralPrevalence) {
    std::unordered_map<Identifier, std::unordered_map<Gender, double>> empty_rural_prevalence;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, ranges_, lambda_, stddev_, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, empty_rural_prevalence,
            income_models_, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionEmptyIncomeModels) {
    std::unordered_map<Income, LinearModelParams> empty_income_models;
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, ranges_, lambda_, stddev_, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr, info_speed_, rural_prevalence_,
            empty_income_models, physical_activity_stddev_, TrendType::Null,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionTrendValidation) {
    // Test trend validation for TrendType::Trend
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, ranges_, lambda_, stddev_, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr,  // Empty trend models for Trend type
            info_speed_, rural_prevalence_, income_models_, physical_activity_stddev_,
            TrendType::Trend, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}

TEST_F(StaticLinearModelTest, StaticLinearModelDefinitionIncomeTrendValidation) {
    // Test income trend validation for TrendType::IncomeTrend
    EXPECT_THROW(
        StaticLinearModelDefinition(
            std::make_shared<RiskFactorSexAgeTable>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            std::make_shared<std::unordered_map<Identifier, int>>(),
            std::make_shared<std::unordered_map<Identifier, double>>(),
            names_, models_, ranges_, lambda_, stddev_, cholesky_,
            policy_models_, policy_ranges_, policy_cholesky_,
            nullptr, nullptr, nullptr,
            info_speed_, rural_prevalence_, income_models_, physical_activity_stddev_,
            TrendType::IncomeTrend, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true),
        HgpsException);
}
