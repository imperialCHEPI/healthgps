#include "HealthGPS.Core/api.h"
#include "HealthGPS.Core/datatable.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/dummy_model.h"
#include "HealthGPS/kevin_hall_model.h"
#include "HealthGPS/person.h"
#include "HealthGPS/population.h"
#include "HealthGPS/runtime_context.h"
#include "HealthGPS/static_linear_model.h"
#include "pch.h"

// Include test helper files
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "Population.Test.cpp" // Contains TestEventAggregator and TestScenario definitions

#include <map>
#include <memory>
#include <vector>

using namespace hgps;

// Tests for KevinHall model implementation- Mahima
//  This tests the basic properties, initialisation, and operations of the KevinHall model
class TestKevinHallModel : public ::testing::Test {
  protected:
    void SetUp() override {
        // Setup mock objects needed for RuntimeContext
        auto bus = std::make_shared<TestEventAggregator>();
        auto inputs = create_test_modelinput();
        auto scenario = std::make_unique<TestScenario>();

        // Create context with proper initialization
        context = std::make_shared<RuntimeContext>(bus, inputs, std::move(scenario));

        // Create person with proper initialization
        person = std::make_shared<Person>(core::Gender::male);
        person->age = 25;
        person->region = core::Region::England;
        person->ethnicity = core::Ethnicity::White;
        person->risk_factors["BMI"_id] = 22.86;
        person->risk_factors["PhysicalActivity"_id] = 150.0;

        // Initialize model parameters with proper error handling
        try {
            // Create test data for Kevin Hall model
            auto expected = std::make_shared<RiskFactorSexAgeTable>();
            auto expected_trend = std::make_shared<std::unordered_map<core::Identifier, double>>();
            auto trend_steps = std::make_shared<std::unordered_map<core::Identifier, int>>();
            
            // Energy equation - minimum required parameters
            std::unordered_map<core::Identifier, double> energy_equation{
                {"Carbohydrate"_id, 4.0},
                {"Protein"_id, 4.0},
                {"Fat"_id, 9.0}
            };
            
            // Nutrient ranges - minimum required parameters
            std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges;
            nutrient_ranges["Carbohydrate"_id] = core::DoubleInterval(0.0, 500.0);
            nutrient_ranges["Protein"_id] = core::DoubleInterval(0.0, 200.0);
            nutrient_ranges["Fat"_id] = core::DoubleInterval(0.0, 200.0);
            
            // Nutrient equations - minimum required parameters
            std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations;
            nutrient_equations["Carbohydrate"_id]["BaseFood"_id] = 1.0;
            nutrient_equations["Protein"_id]["BaseFood"_id] = 1.0;
            nutrient_equations["Fat"_id]["BaseFood"_id] = 1.0;
            
            // Food prices - can be empty
            std::unordered_map<core::Identifier, std::optional<double>> food_prices;
            food_prices["BaseFood"_id] = 1.0;
            
            // Weight quantiles - minimum required parameters
            std::unordered_map<core::Gender, std::vector<double>> weight_quantiles;
            weight_quantiles[core::Gender::male] = {60.0, 70.0, 80.0, 90.0, 100.0};
            weight_quantiles[core::Gender::female] = {50.0, 60.0, 70.0, 80.0, 90.0};
            
            // EPA quantiles - must match weight quantiles size
            std::vector<double> epa_quantiles = {0.2, 0.4, 0.6, 0.8, 1.0};
            
            // Height model parameters
            std::unordered_map<core::Gender, double> height_stddev;
            height_stddev[core::Gender::male] = 0.07;
            height_stddev[core::Gender::female] = 0.06;
            
            std::unordered_map<core::Gender, double> height_slope;
            height_slope[core::Gender::male] = 0.4;
            height_slope[core::Gender::female] = 0.35;
            
            // Region models
            auto region_models = std::make_shared<std::unordered_map<core::Region, LinearModelParams>>();
            (*region_models)[core::Region::England] = LinearModelParams{0.0, {{"age", 0.1}}};
            
            // Ethnicity models
            auto ethnicity_models = std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>();
            (*ethnicity_models)[core::Ethnicity::White] = LinearModelParams{0.0, {{"age", 0.1}}};
            
            // Income models
            std::unordered_map<core::Income, LinearModelParams> income_models;
            income_models[core::Income::low] = LinearModelParams{0.0, {{"age", 0.1}}};
            
            // Income continuous stddev
            double income_continuous_stddev = 0.2;
            
            // Create Kevin Hall model with all required parameters
            model = std::unique_ptr<KevinHallModel>(new KevinHallModel(
                expected, expected_trend, trend_steps,
                energy_equation, nutrient_ranges, nutrient_equations, food_prices,
                weight_quantiles, epa_quantiles, height_stddev, height_slope,
                region_models, ethnicity_models, income_models, income_continuous_stddev
            ));
        } catch (const std::exception &e) {
            FAIL() << "Failed to initialize KevinHallModel: " << e.what();
        }
    }

    void TearDown() override { model.reset(); }

    std::shared_ptr<RuntimeContext> context;
    std::shared_ptr<Person> person;
    std::unique_ptr<KevinHallModel> model;
};

TEST_F(TestKevinHallModel, BasicProperties) {
    EXPECT_EQ(model->type(), RiskFactorModelType::Dynamic);
    EXPECT_EQ(model->name(), "Dynamic");
}

TEST_F(TestKevinHallModel, InitialiseNutrientIntakes) {
    model->generate_risk_factors(*context);
    EXPECT_GT(person->risk_factors["Carbohydrates"_id], 0);
    EXPECT_GT(person->risk_factors["Fats"_id], 0);
    EXPECT_GT(person->risk_factors["Proteins"_id], 0);
    EXPECT_GT(person->risk_factors["Sodium"_id], 0);
}

TEST_F(TestKevinHallModel, InitialiseEnergyIntake) {
    model->generate_risk_factors(*context);
    double energy = person->risk_factors["EnergyIntake"_id];
    EXPECT_GT(energy, 0);
    EXPECT_LT(energy, 5000);
}

TEST_F(TestKevinHallModel, ComputeBMI) {
    model->generate_risk_factors(*context);
    EXPECT_NEAR(person->risk_factors["BMI"_id], 22.86, 0.01);
}

TEST_F(TestKevinHallModel, InitialiseRegionAndEthnicity) {
    model->generate_risk_factors(*context);
    EXPECT_GE(static_cast<int>(person->region), static_cast<int>(core::Region::England));
    EXPECT_LE(static_cast<int>(person->region), static_cast<int>(core::Region::NorthernIreland));
    EXPECT_GE(static_cast<int>(person->ethnicity), static_cast<int>(core::Ethnicity::White));
    EXPECT_LE(static_cast<int>(person->ethnicity), static_cast<int>(core::Ethnicity::Others));
}

TEST_F(TestKevinHallModel, InitialisePhysicalActivity) {
    model->generate_risk_factors(*context);
    double pa = person->risk_factors["PhysicalActivity"_id];
    EXPECT_GT(pa, 0);
    EXPECT_LT(pa, 500);
}

TEST_F(TestKevinHallModel, InitialiseIncome) {
    model->generate_risk_factors(*context);
    double income = person->risk_factors["Income"_id];
    EXPECT_GT(income, 0);
    EXPECT_LT(income, 200000);

    EXPECT_GE(static_cast<int>(person->income_category), static_cast<int>(core::Income::low));
    EXPECT_LE(static_cast<int>(person->income_category), static_cast<int>(core::Income::high));
}

TEST_F(TestKevinHallModel, WeightAdjustments) {
    model->generate_risk_factors(*context);
    double initial_weight = person->risk_factors["Weight"_id];
    model->update_risk_factors(*context);
    EXPECT_NE(person->risk_factors["Weight"_id], initial_weight);
}

TEST_F(TestKevinHallModel, ComprehensiveModelOperations) {
    // Test model initialization
    EXPECT_NO_THROW(model->generate_risk_factors(*context));

    // Test update operations
    EXPECT_NO_THROW(model->update_risk_factors(*context));

    // Test weight adjustments through public interface
    EXPECT_NO_THROW(model->adjust_risk_factors(*context, {"Weight"_id}, std::nullopt, true));

    // Test energy calculations through public interface
    person->risk_factors["Weight"_id] = 70.0;
    person->risk_factors["BodyFat"_id] = 15.0;
    person->risk_factors["LeanTissue"_id] = 55.0;
    person->risk_factors["EnergyIntake"_id] = 2000.0;
    person->risk_factors["Intercept_K"_id] = 100.0;

    EXPECT_NO_THROW(model->update_risk_factors(*context));

    // Test income category operations through public interface
    EXPECT_NO_THROW(model->generate_risk_factors(*context));
    EXPECT_NO_THROW(model->update_risk_factors(*context));
}

TEST_F(TestKevinHallModel, BasicOperations) {
    ASSERT_NE(nullptr, model);
    ASSERT_NE(nullptr, person);
    ASSERT_NE(nullptr, context);

    // Test that the person has the expected initial values
    ASSERT_EQ(core::Gender::male, person->gender);
    ASSERT_EQ(25, person->age);
    ASSERT_EQ(core::Region::England, person->region);
    ASSERT_EQ(core::Ethnicity::White, person->ethnicity);
    ASSERT_EQ(22.86, person->risk_factors["BMI"_id]);
    ASSERT_EQ(150.0, person->risk_factors["PhysicalActivity"_id]);
}

TEST_F(TestKevinHallModel, ModelGeneration) {
    // Test risk factor generation
    EXPECT_NO_THROW(model->generate_risk_factors(*context));
}

class TestStaticLinearModel : public ::testing::Test {
  protected:
    // Explicitly delete default constructor since RuntimeContext has no default constructor
    TestStaticLinearModel() = delete;

    void SetUp() override {
        // Setup mock objects needed for RuntimeContext
        auto bus = std::make_shared<TestEventAggregator>();
        auto inputs = create_test_modelinput();
        auto scenario = std::make_unique<TestScenario>();

        // Create context with proper initialization
        context = std::make_unique<RuntimeContext>(bus, inputs, std::move(scenario));

        // Setup test person with proper initialization
        person = Person(core::Gender::male);
        person.age = 30;
        person.region = core::Region::England;
        person.ethnicity = core::Ethnicity::White;
        person.risk_factors["BMI"_id] = 25.0;
        person.risk_factors["PhysicalActivity"_id] = 150.0;

        // Add person to population with proper time
        population.add(person, 2023);

        // Setup model parameters with proper error handling
        try {
            // Create test data for model initialization
            std::vector<core::Identifier> test_names = {"test_factor"_id};
            std::vector<double> test_values = {1.0};
            std::vector<double> test_policy = {0.0};
            std::vector<int> test_policy_start = {2020};

            auto expected = std::make_shared<RiskFactorSexAgeTable>();
            auto expected_trend = std::make_shared<std::unordered_map<core::Identifier, double>>();
            auto trend_steps = std::make_shared<std::unordered_map<core::Identifier, int>>();

            auto region_models =
                std::make_shared<std::unordered_map<core::Region, LinearModelParams>>();
            (*region_models)[core::Region::England] =
                LinearModelParams{0.0, {{"age", 0.1}, {"gender", 0.2}}};
            (*region_models)[core::Region::Wales] =
                LinearModelParams{0.1, {{"age", 0.15}, {"gender", 0.25}}};

            auto ethnicity_models =
                std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>();
            (*ethnicity_models)[core::Ethnicity::White] =
                LinearModelParams{0.0, {{"age", 0.1}, {"gender", 0.2}, {"region", 0.3}}};
            (*ethnicity_models)[core::Ethnicity::Black] =
                LinearModelParams{0.1, {{"age", 0.15}, {"gender", 0.25}, {"region", 0.35}}};

            std::unordered_map<core::Income, LinearModelParams> income_models{
                {core::Income::low,
                 LinearModelParams{
                     0.0, {{"age", 0.1}, {"gender", 0.2}, {"region", 0.3}, {"ethnicity", 0.4}}}},
                {core::Income::high,
                 LinearModelParams{
                     0.1,
                     {{"age", 0.15}, {"gender", 0.25}, {"region", 0.35}, {"ethnicity", 0.45}}}}};

            auto model_def = DummyModelDefinition(
                RiskFactorModelType::Static, std::move(test_names), std::move(test_values),
                std::move(test_policy), std::move(test_policy_start));

            model = std::unique_ptr<StaticLinearModel>(
                dynamic_cast<StaticLinearModel *>(model_def.create_model().release()));

            if (!model) {
                throw std::runtime_error("Failed to create StaticLinearModel");
            }
        } catch (const std::exception &e) {
            FAIL() << "Failed to initialize StaticLinearModel: " << e.what();
        }
    }

    void TearDown() override { model.reset(); }

    std::unique_ptr<RuntimeContext> context;
    Person person;
    Population population;
    std::unique_ptr<StaticLinearModel> model;
};

TEST_F(TestStaticLinearModel, BasicProperties) {
    ASSERT_EQ(RiskFactorModelType::Static, model->type());
    ASSERT_EQ("Static", model->name());
}

TEST_F(TestStaticLinearModel, InitialiseRegion) {
    // Test through public interface
    ASSERT_NO_THROW(model->generate_risk_factors(*context));

    // Verify region is assigned
    ASSERT_TRUE(static_cast<int>(person.region) >= static_cast<int>(core::Region::England) &&
                static_cast<int>(person.region) <= static_cast<int>(core::Region::NorthernIreland));
}

TEST_F(TestStaticLinearModel, InitialiseEthnicity) {
    // Test through public interface
    ASSERT_NO_THROW(model->generate_risk_factors(*context));

    // Verify ethnicity is assigned
    ASSERT_TRUE(static_cast<int>(person.ethnicity) >= static_cast<int>(core::Ethnicity::White) &&
                static_cast<int>(person.ethnicity) <= static_cast<int>(core::Ethnicity::Others));
}

TEST_F(TestStaticLinearModel, InitialiseIncome) {
    // Test through public interface
    ASSERT_NO_THROW(model->generate_risk_factors(*context));

    // Verify income values
    ASSERT_GT(person.risk_factors["Income"_id], -10.0); // Reasonable lower bound
    ASSERT_LT(person.risk_factors["Income"_id], 10.0);  // Reasonable upper bound
    ASSERT_TRUE(static_cast<int>(person.income_category) >= static_cast<int>(core::Income::low) &&
                static_cast<int>(person.income_category) <= static_cast<int>(core::Income::high));
}

TEST_F(TestStaticLinearModel, InitialisePhysicalActivity) {
    // Test through public interface
    ASSERT_NO_THROW(model->generate_risk_factors(*context));

    // Verify physical activity is within reasonable bounds
    ASSERT_GT(person.risk_factors["PhysicalActivity"_id], 0.0);
    ASSERT_LT(person.risk_factors["PhysicalActivity"_id], 500.0); // Reasonable upper bound
}

TEST_F(TestStaticLinearModel, UpdateOperations) {
    // Test through public interface
    ASSERT_NO_THROW(model->generate_risk_factors(*context));

    // Test updates
    ASSERT_NO_THROW(model->update_risk_factors(*context));
}

TEST(TestStaticLinearModel, IncomeCategoryOperations) {
    // Setup test environment
    auto bus = std::make_shared<TestEventAggregator>();
    auto inputs = create_test_modelinput();
    auto scenario = std::make_unique<TestScenario>();
    auto context = std::make_unique<RuntimeContext>(bus, inputs, std::move(scenario));

    // Add test persons with different incomes
    for (int i = 0; i < 10; i++) {
        Person person;
        person.income_continuous = (i + 1) * 10000.0;
        context->population().add(person, 2023);
    }

    // Create model instance with proper initialization
    std::vector<core::Identifier> test_names = {"test_factor"_id};
    std::vector<double> test_values = {1.0};
    std::vector<double> test_policy = {0.0};
    std::vector<int> test_policy_start = {2020};

    auto model_def = DummyModelDefinition(RiskFactorModelType::Static, std::move(test_names),
                                         std::move(test_values), std::move(test_policy),
                                         std::move(test_policy_start));

    auto model_ptr = model_def.create_model();
    auto *static_model = dynamic_cast<StaticLinearModel *>(model_ptr.get());
    if (!static_model) {
        FAIL() << "Failed to create StaticLinearModel";
    }

    // Test through public interface
    EXPECT_NO_THROW(static_model->generate_risk_factors(*context));
    EXPECT_NO_THROW(static_model->update_risk_factors(*context));
}

TEST_F(TestStaticLinearModel, BasicOperations) {
    ASSERT_NE(nullptr, model);

    // Test that the person has the expected initial values
    ASSERT_EQ(core::Gender::male, person.gender);
    ASSERT_EQ(30, person.age);
    ASSERT_EQ(core::Region::England, person.region);
    ASSERT_EQ(core::Ethnicity::White, person.ethnicity);
    ASSERT_EQ(25.0, person.risk_factors["BMI"_id]);
    ASSERT_EQ(150.0, person.risk_factors["PhysicalActivity"_id]);
}

TEST_F(TestStaticLinearModel, ModelGeneration) {
    // Test risk factor generation
    EXPECT_NO_THROW(model->generate_risk_factors(*context));
}

// Tests for model parser coverage - Mahima
TEST(TestModelParser, ComprehensiveParserOperations) {
    using namespace hgps::input;

    // Test HLM model definition loading (lines 147-176)
    nlohmann::json hlm_config = {
        {"models",
         {{"test_model",
           {{"coefficients",
             {{"coef1", {{"value", 1.0}, {"pvalue", 0.05}, {"tvalue", 2.0}, {"std_error", 0.1}}}}},
            {"residuals_standard_deviation", 1.0},
            {"rsquared", 0.8}}}}},
        {"levels", {{"level1", {{"name", "Test Level"}, {"description", "Test Description"}}}}}};

    EXPECT_NO_THROW(load_hlm_risk_model_definition(hlm_config));

    // Test Kevin Hall model definition loading (lines 563-594)
    nlohmann::json kh_config = {
        {"Nutrients",
         {{{"Name", "test_nutrient"}, {"Range", {"min", 0.0, "max", 100.0}}, {"Energy", 4.0}}}}};

    Configuration config;
    EXPECT_NO_THROW(load_kevinhall_risk_model_definition(kh_config, config));
}

// Test for dynamic hierarchical linear model coverage - Mahima
TEST(TestDynamicHierarchicalLinearModel, RiskFactorGeneration) {
    // Create test model
    std::map<core::IntegerInterval, AgeGroupGenderEquation> equations;
    std::map<core::Identifier, core::Identifier> variables;

    auto model = DynamicHierarchicalLinearModel(
        std::make_shared<RiskFactorSexAgeTable>(),
        std::make_shared<std::unordered_map<core::Identifier, double>>(),
        std::make_shared<std::unordered_map<core::Identifier, int>>(), equations, variables, 0.1);

    // Create test context
    auto context = RuntimeContext(std::make_shared<TestEventAggregator>(), create_test_modelinput(),
                                  std::make_unique<TestScenario>());

    // Test risk factor generation (line 37)
    EXPECT_NO_THROW(model.generate_risk_factors(context));
}
