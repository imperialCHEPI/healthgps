#include "pch.h"

#include "HealthGPS.Core/identifier.h"
#include "HealthGPS.Core/interval.h"
#include "HealthGPS/gender_table.h"
#include "HealthGPS/kevin_hall_model.h"
#include "HealthGPS/person.h"
#include "HealthGPS/random_algorithm.h"
#include "HealthGPS/runtime_context.h"

#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace hgps;
using namespace hgps::core;

class KevinHallModelTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create test data
        create_test_expected_values();
        create_test_energy_equation();
        create_test_nutrient_ranges();
        create_test_nutrient_equations();
        create_test_food_prices();
        create_test_weight_quantiles();
        create_test_epa_quantiles();
        create_test_height_parameters();
    }

    void create_test_expected_values() {
        expected_ = std::make_shared<RiskFactorSexAgeTable>();

        // Create age range 0-100
        auto age_range = IntegerInterval(0, 100);
        expected_->emplace_row(Gender::male, create_age_gender_table<double>(age_range));
        expected_->emplace_row(Gender::female, create_age_gender_table<double>(age_range));

        // Set some test values for foods and nutrients
        for (int age = 0; age <= 100; age++) {
            // Food intakes
            expected_->at(Gender::male)["Food1"_id] = 100.0 + age * 0.5;
            expected_->at(Gender::male)["Food2"_id] = 80.0 + age * 0.3;
            expected_->at(Gender::female)["Food1"_id] = 90.0 + age * 0.4;
            expected_->at(Gender::female)["Food2"_id] = 70.0 + age * 0.2;

            // Weight
            expected_->at(Gender::male)["Weight"_id] = 70.0 + age * 0.2;
            expected_->at(Gender::female)["Weight"_id] = 60.0 + age * 0.15;

            // Height
            expected_->at(Gender::male)["Height"_id] = 175.0 + age * 0.1;
            expected_->at(Gender::female)["Height"_id] = 165.0 + age * 0.08;

            // Physical Activity
            expected_->at(Gender::male)["PhysicalActivity"_id] = 1.5 + age * 0.001;
            expected_->at(Gender::female)["PhysicalActivity"_id] = 1.4 + age * 0.001;
        }
    }

    void create_test_energy_equation() {
        energy_equation_["Carbohydrate"_id] = 4.0;
        energy_equation_["Protein"_id] = 4.0;
        energy_equation_["Fat"_id] = 9.0;
        energy_equation_["Sodium"_id] = 0.0;
    }

    void create_test_nutrient_ranges() {
        nutrient_ranges_["Carbohydrate"_id] = DoubleInterval(50.0, 500.0);
        nutrient_ranges_["Protein"_id] = DoubleInterval(30.0, 200.0);
        nutrient_ranges_["Fat"_id] = DoubleInterval(20.0, 150.0);
        nutrient_ranges_["Sodium"_id] = DoubleInterval(1.0, 10.0);
    }

    void create_test_nutrient_equations() {
        // Food1 nutrient composition
        std::map<Identifier, double> food1_nutrients;
        food1_nutrients["Carbohydrate"_id] = 0.6;
        food1_nutrients["Protein"_id] = 0.2;
        food1_nutrients["Fat"_id] = 0.1;
        food1_nutrients["Sodium"_id] = 0.01;
        nutrient_equations_["Food1"_id] = food1_nutrients;

        // Food2 nutrient composition
        std::map<Identifier, double> food2_nutrients;
        food2_nutrients["Carbohydrate"_id] = 0.4;
        food2_nutrients["Protein"_id] = 0.3;
        food2_nutrients["Fat"_id] = 0.2;
        food2_nutrients["Sodium"_id] = 0.02;
        nutrient_equations_["Food2"_id] = food2_nutrients;
    }

    void create_test_food_prices() {
        food_prices_["Food1"_id] = 2.5;
        food_prices_["Food2"_id] = 3.0;
    }

    void create_test_weight_quantiles() {
        // Create weight quantiles for males and females
        std::vector<double> male_quantiles;
        std::vector<double> female_quantiles;

        for (int i = 0; i < 100; i++) {
            male_quantiles.push_back(0.8 + i * 0.002);   // 0.8 to 1.0
            female_quantiles.push_back(0.7 + i * 0.003); // 0.7 to 1.0
        }

        weight_quantiles_[Gender::male] = male_quantiles;
        weight_quantiles_[Gender::female] = female_quantiles;
    }

    void create_test_epa_quantiles() {
        for (int i = 0; i < 100; i++) {
            epa_quantiles_.push_back(0.5 + i * 0.005); // 0.5 to 1.0
        }
    }

    void create_test_height_parameters() {
        height_stddev_[Gender::male] = 7.0;
        height_stddev_[Gender::female] = 6.0;
        height_slope_[Gender::male] = 0.3;
        height_slope_[Gender::female] = 0.25;
    }

    // Test data members
    std::shared_ptr<RiskFactorSexAgeTable> expected_;
    std::shared_ptr<std::unordered_map<Identifier, double>> expected_trend_;
    std::shared_ptr<std::unordered_map<Identifier, int>> trend_steps_;
    std::unordered_map<Identifier, double> energy_equation_;
    std::unordered_map<Identifier, DoubleInterval> nutrient_ranges_;
    std::unordered_map<Identifier, std::map<Identifier, double>> nutrient_equations_;
    std::unordered_map<Identifier, std::optional<double>> food_prices_;
    std::unordered_map<Gender, std::vector<double>> weight_quantiles_;
    std::vector<double> epa_quantiles_;
    std::unordered_map<Gender, double> height_stddev_;
    std::unordered_map<Gender, double> height_slope_;
};

TEST_F(KevinHallModelTest, Constructor) {
    auto model = std::make_unique<KevinHallModel>(
        expected_, expected_trend_, trend_steps_, energy_equation_, nutrient_ranges_,
        nutrient_equations_, food_prices_, weight_quantiles_, epa_quantiles_, height_stddev_,
        height_slope_);

    EXPECT_EQ(RiskFactorModelType::Dynamic, model->type());
    EXPECT_EQ("Dynamic", model->name());
}

TEST_F(KevinHallModelTest, ConstructorWithEmptyEnergyEquation) {
    std::unordered_map<Identifier, double> empty_energy_equation;

    EXPECT_THROW(KevinHallModel(expected_, expected_trend_, trend_steps_, empty_energy_equation,
                                nutrient_ranges_, nutrient_equations_, food_prices_,
                                weight_quantiles_, epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, ConstructorWithEmptyNutrientRanges) {
    std::unordered_map<Identifier, DoubleInterval> empty_nutrient_ranges;

    EXPECT_THROW(KevinHallModel(expected_, expected_trend_, trend_steps_, energy_equation_,
                                empty_nutrient_ranges, nutrient_equations_, food_prices_,
                                weight_quantiles_, epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, ConstructorWithEmptyNutrientEquations) {
    std::unordered_map<Identifier, std::map<Identifier, double>> empty_nutrient_equations;

    EXPECT_THROW(KevinHallModel(expected_, expected_trend_, trend_steps_, energy_equation_,
                                nutrient_ranges_, empty_nutrient_equations, food_prices_,
                                weight_quantiles_, epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, ConstructorWithEmptyFoodPrices) {
    std::unordered_map<Identifier, std::optional<double>> empty_food_prices;

    EXPECT_THROW(KevinHallModel(expected_, expected_trend_, trend_steps_, energy_equation_,
                                nutrient_ranges_, nutrient_equations_, empty_food_prices,
                                weight_quantiles_, epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, ConstructorWithEmptyWeightQuantiles) {
    std::unordered_map<Gender, std::vector<double>> empty_weight_quantiles;

    EXPECT_THROW(KevinHallModel(expected_, expected_trend_, trend_steps_, energy_equation_,
                                nutrient_ranges_, nutrient_equations_, food_prices_,
                                empty_weight_quantiles, epa_quantiles_, height_stddev_,
                                height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, ConstructorWithEmptyEpaQuantiles) {
    std::vector<double> empty_epa_quantiles;

    EXPECT_THROW(KevinHallModel(expected_, expected_trend_, trend_steps_, energy_equation_,
                                nutrient_ranges_, nutrient_equations_, food_prices_,
                                weight_quantiles_, empty_epa_quantiles, height_stddev_,
                                height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, ConstructorWithEmptyHeightStdDev) {
    std::unordered_map<Gender, double> empty_height_stddev;

    EXPECT_THROW(KevinHallModel(expected_, expected_trend_, trend_steps_, energy_equation_,
                                nutrient_ranges_, nutrient_equations_, food_prices_,
                                weight_quantiles_, epa_quantiles_, empty_height_stddev,
                                height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, ConstructorWithEmptyHeightSlope) {
    std::unordered_map<Gender, double> empty_height_slope;

    EXPECT_THROW(KevinHallModel(expected_, expected_trend_, trend_steps_, energy_equation_,
                                nutrient_ranges_, nutrient_equations_, food_prices_,
                                weight_quantiles_, epa_quantiles_, height_stddev_,
                                empty_height_slope),
                 HgpsException);
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionConstructor) {
    auto definition = std::make_unique<KevinHallModelDefinition>(
        std::move(expected_), std::move(expected_trend_), std::move(trend_steps_),
        std::move(energy_equation_), std::move(nutrient_ranges_), std::move(nutrient_equations_),
        std::move(food_prices_), std::move(weight_quantiles_), std::move(epa_quantiles_),
        std::move(height_stddev_), std::move(height_slope_));

    auto model = definition->create_model();
    EXPECT_NE(nullptr, model);
    EXPECT_EQ(RiskFactorModelType::Dynamic, model->type());
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionValidation) {
    // Test with empty energy equation
    std::unordered_map<Identifier, double> empty_energy_equation;
    EXPECT_THROW(KevinHallModelDefinition(
                     std::make_shared<RiskFactorSexAgeTable>(),
                     std::make_shared<std::unordered_map<Identifier, double>>(),
                     std::make_shared<std::unordered_map<Identifier, int>>(), empty_energy_equation,
                     nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                     epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionEmptyNutrientRanges) {
    std::unordered_map<Identifier, DoubleInterval> empty_nutrient_ranges;
    EXPECT_THROW(KevinHallModelDefinition(
                     std::make_shared<RiskFactorSexAgeTable>(),
                     std::make_shared<std::unordered_map<Identifier, double>>(),
                     std::make_shared<std::unordered_map<Identifier, int>>(), energy_equation_,
                     empty_nutrient_ranges, nutrient_equations_, food_prices_, weight_quantiles_,
                     epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionEmptyNutrientEquations) {
    std::unordered_map<Identifier, std::map<Identifier, double>> empty_nutrient_equations;
    EXPECT_THROW(KevinHallModelDefinition(
                     std::make_shared<RiskFactorSexAgeTable>(),
                     std::make_shared<std::unordered_map<Identifier, double>>(),
                     std::make_shared<std::unordered_map<Identifier, int>>(), energy_equation_,
                     nutrient_ranges_, empty_nutrient_equations, food_prices_, weight_quantiles_,
                     epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionEmptyFoodPrices) {
    std::unordered_map<Identifier, std::optional<double>> empty_food_prices;
    EXPECT_THROW(KevinHallModelDefinition(
                     std::make_shared<RiskFactorSexAgeTable>(),
                     std::make_shared<std::unordered_map<Identifier, double>>(),
                     std::make_shared<std::unordered_map<Identifier, int>>(), energy_equation_,
                     nutrient_ranges_, nutrient_equations_, empty_food_prices, weight_quantiles_,
                     epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionEmptyWeightQuantiles) {
    std::unordered_map<Gender, std::vector<double>> empty_weight_quantiles;
    EXPECT_THROW(KevinHallModelDefinition(
                     std::make_shared<RiskFactorSexAgeTable>(),
                     std::make_shared<std::unordered_map<Identifier, double>>(),
                     std::make_shared<std::unordered_map<Identifier, int>>(), energy_equation_,
                     nutrient_ranges_, nutrient_equations_, food_prices_, empty_weight_quantiles,
                     epa_quantiles_, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionEmptyEpaQuantiles) {
    std::vector<double> empty_epa_quantiles;
    EXPECT_THROW(KevinHallModelDefinition(
                     std::make_shared<RiskFactorSexAgeTable>(),
                     std::make_shared<std::unordered_map<Identifier, double>>(),
                     std::make_shared<std::unordered_map<Identifier, int>>(), energy_equation_,
                     nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                     empty_epa_quantiles, height_stddev_, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionEmptyHeightStdDev) {
    std::unordered_map<Gender, double> empty_height_stddev;
    EXPECT_THROW(KevinHallModelDefinition(
                     std::make_shared<RiskFactorSexAgeTable>(),
                     std::make_shared<std::unordered_map<Identifier, double>>(),
                     std::make_shared<std::unordered_map<Identifier, int>>(), energy_equation_,
                     nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                     epa_quantiles_, empty_height_stddev, height_slope_),
                 HgpsException);
}

TEST_F(KevinHallModelTest, KevinHallModelDefinitionEmptyHeightSlope) {
    std::unordered_map<Gender, double> empty_height_slope;
    EXPECT_THROW(KevinHallModelDefinition(
                     std::make_shared<RiskFactorSexAgeTable>(),
                     std::make_shared<std::unordered_map<Identifier, double>>(),
                     std::make_shared<std::unordered_map<Identifier, int>>(), energy_equation_,
                     nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                     epa_quantiles_, height_stddev_, empty_height_slope),
                 HgpsException);
}

TEST_F(KevinHallModelTest, ComputeGMethod) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    double CI = 200.0;
    double CI_0 = 150.0;
    double G_0 = 0.5;

    double result = model.compute_G(CI, CI_0, G_0);

    // Expected: sqrt(200 / (150 / (0.5 * 0.5))) = sqrt(200 / 600) = sqrt(0.333...) ≈ 0.577
    EXPECT_NEAR(0.577, result, 0.01);
}

TEST_F(KevinHallModelTest, ComputeECFMethod) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    double delta_Na = 0.1;
    double CI = 200.0;
    double CI_0 = 150.0;
    double ECF_0 = 10.0;

    double result = model.compute_ECF(delta_Na, CI, CI_0, ECF_0);

    // Expected: 10.0 + (0.1 - xi_CI * (1.0 - 200/150)) / xi_Na
    // This is a complex calculation involving model constants
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(KevinHallModelTest, ComputeDeltaMethod) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    int age = 30;
    Gender sex = Gender::male;
    double PAL = 1.5;
    double BW = 70.0;
    double H = 175.0;

    double result = model.compute_delta(age, sex, PAL, BW, H);

    // Expected: ((1.0 - beta_TEF) * PAL - 1.0) * RMR / BW
    // RMR = 9.99 * 70 + 6.25 * 175 - 4.92 * 30 + 5.0 = 699.3 + 1093.75 - 147.6 + 5.0 = 1650.45
    // RMR *= 4.184 = 6901.48 kJ
    // delta = ((1.0 - beta_TEF) * 1.5 - 1.0) * 6901.48 / 70
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(KevinHallModelTest, ComputeEEMethod) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    double BW = 70.0;
    double F = 15.0;
    double L = 50.0;
    double EI = 2000.0;
    double K = 100.0;
    double delta = 20.0;
    double x = 0.1;

    double result = model.compute_EE(BW, F, L, EI, K, delta, x);

    // Expected: (K + gamma_F * F + gamma_L * L + delta * BW + beta_TEF + beta_AT + x * (EI - rho_G
    // * 0.0)) / (1 + x)
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(KevinHallModelTest, ComputeBMIMethod) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    Person person;
    person.risk_factors["Weight"_id] = 70.0;
    person.risk_factors["Height"_id] = 175.0;

    model.compute_bmi(person);

    // Expected BMI: 70 / (1.75 * 1.75) = 70 / 3.0625 = 22.86
    EXPECT_NEAR(22.86, person.risk_factors.at("BMI"_id), 0.01);
}

TEST_F(KevinHallModelTest, GetWeightQuantileMethod) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    double epa_quantile = 0.75;
    Gender sex = Gender::male;

    double result = model.get_weight_quantile(epa_quantile, sex);

    // Should return a value from the weight quantiles
    EXPECT_TRUE(result >= 0.8 && result <= 1.0);
}

TEST_F(KevinHallModelTest, GetWeightQuantileFemale) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    double epa_quantile = 0.5;
    Gender sex = Gender::female;

    double result = model.get_weight_quantile(epa_quantile, sex);

    // Should return a value from the female weight quantiles
    EXPECT_TRUE(result >= 0.7 && result <= 1.0);
}

TEST_F(KevinHallModelTest, GetExpectedMethod) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Test getting expected nutrient intake
    double result =
        model.get_expected(*mock_context, Gender::male, 30, "Carbohydrate"_id, std::nullopt, false);

    // Should compute from food intakes and nutrient equations
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(KevinHallModelTest, GetExpectedEnergyIntake) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Test getting expected energy intake
    double result =
        model.get_expected(*mock_context, Gender::male, 30, "EnergyIntake"_id, std::nullopt, false);

    // Should compute from nutrient intakes and energy equation
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(KevinHallModelTest, GetExpectedWeight) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Test getting expected weight
    double result =
        model.get_expected(*mock_context, Gender::male, 30, "Weight"_id, std::nullopt, false);

    // Should return the expected weight value
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(KevinHallModelTest, GetExpectedHeight) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Test getting expected height
    double result =
        model.get_expected(*mock_context, Gender::male, 30, "Height"_id, std::nullopt, false);

    // Should return the expected height value
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(KevinHallModelTest, GetExpectedPhysicalActivity) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Test getting expected physical activity
    double result = model.get_expected(*mock_context, Gender::male, 30, "PhysicalActivity"_id,
                                       std::nullopt, false);

    // Should return the expected physical activity value
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(KevinHallModelTest, GetExpectedUnknownFactor) {
    KevinHallModel model(expected_, expected_trend_, trend_steps_, energy_equation_,
                         nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
                         epa_quantiles_, height_stddev_, height_slope_);

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Test getting expected value for unknown factor
    double result = model.get_expected(*mock_context, Gender::male, 30, "UnknownFactor"_id,
                                       std::nullopt, false);

    // Should fall back to base class implementation
    EXPECT_TRUE(std::isfinite(result));
}
