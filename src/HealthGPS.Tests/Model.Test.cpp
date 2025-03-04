#include "HealthGPS.Core/api.h"
#include "HealthGPS.Core/datatable.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/kevin_hall_model.h"
#include "HealthGPS/person.h"
#include "HealthGPS/population.h"
#include "HealthGPS/runtime_context.h"
#include "HealthGPS/static_linear_model.h"
#include "pch.h"

#include <map>
#include <memory>
#include <vector>

using namespace hgps;

class TestKevinHallModel : public ::testing::Test {
  protected:
    void SetUp() override {
        // Setup mock context and test person
        context = std::make_shared<RuntimeContext>();
        person = std::make_shared<Person>(Gender::Male);
        person->set_age(25);
        person->set_region(1);
        person->set_ethnicity(1);
        person->set_bmi(22.86);
        person->set_physical_activity(150);

        // Initialize model parameters
        auto expected = std::make_shared<RiskFactorSexAgeTable>();
        auto expected_trend = std::make_shared<std::map<std::string, double>>();
        auto trend_steps = std::make_shared<std::map<std::string, double>>();

        std::map<std::string, double> energy_equation;
        std::map<std::string, Interval<double>> nutrient_ranges;
        std::map<std::string, std::map<std::string, double>> nutrient_equations;
        std::map<std::string, double> food_prices;
        std::map<std::string, double> weight_quantiles;
        std::vector<double> epa_quantiles;
        std::map<Gender, double> height_stddev;
        std::map<Gender, double> height_slope;
        std::map<std::string, double> region_models;
        std::map<std::string, double> ethnicity_models;
        std::map<std::string, double> income_models;
        double income_continuous_stddev = 0.5;

        model = std::make_unique<KevinHallModel>(
            expected, expected_trend, trend_steps, energy_equation, nutrient_ranges,
            nutrient_equations, food_prices, weight_quantiles, epa_quantiles, height_stddev,
            height_slope, region_models, ethnicity_models, income_models, income_continuous_stddev);
    }

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
    EXPECT_GT(person->get_carbohydrates(), 0);
    EXPECT_GT(person->get_fats(), 0);
    EXPECT_GT(person->get_proteins(), 0);
    EXPECT_GT(person->get_sodium(), 0);
}

TEST_F(TestKevinHallModel, InitialiseEnergyIntake) {
    model->generate_risk_factors(*context);
    double energy = person->get_energy_intake();
    EXPECT_GT(energy, 0);
    EXPECT_LT(energy, 5000);
}

TEST_F(TestKevinHallModel, ComputeBMI) {
    model->generate_risk_factors(*context);
    EXPECT_NEAR(person->get_bmi(), 22.86, 0.01);
}

TEST_F(TestKevinHallModel, InitialiseRegionAndEthnicity) {
    model->generate_risk_factors(*context);
    EXPECT_GE(person->get_region(), 1);
    EXPECT_LE(person->get_region(), 4);
    EXPECT_GE(person->get_ethnicity(), 1);
    EXPECT_LE(person->get_ethnicity(), 4);
}

TEST_F(TestKevinHallModel, InitialisePhysicalActivity) {
    model->generate_risk_factors(*context);
    double pa = person->get_physical_activity();
    EXPECT_GT(pa, 0);
    EXPECT_LT(pa, 500);
}

TEST_F(TestKevinHallModel, InitialiseIncome) {
    model->generate_risk_factors(*context);
    double income = person->get_income();
    EXPECT_GT(income, 0);
    EXPECT_LT(income, 200000);

    int category = person->get_income_category();
    EXPECT_GE(category, 1);
    EXPECT_LE(category, 5);
}

TEST_F(TestKevinHallModel, WeightAdjustments) {
    model->generate_risk_factors(*context);
    double initial_weight = person->get_weight();
    model->update_risk_factors(*context);
    EXPECT_NE(person->get_weight(), initial_weight);
}
// Test for comprehensive model operations - Mahima
// This covers all the operations in the model for KevinHall
TEST_F(TestKevinHallModel, ComprehensiveModelOperations) {
    // Test model initialization (line 42)
    EXPECT_NO_THROW(model->generate_risk_factors(*context));

    // Test update operations (lines 58, 133, 178)
    EXPECT_NO_THROW(model->update_risk_factors(*context));

    // Test weight adjustments (lines 221, 258)
    auto adjustments = model->compute_weight_adjustments(*context);
    EXPECT_NO_THROW(model->send_weight_adjustments(*context, std::move(adjustments)));

    // Test energy calculations (lines 351, 530, 583, 632)
    person->risk_factors["Weight"_id] = 70.0;
    person->risk_factors["BodyFat"_id] = 15.0;
    person->risk_factors["LeanTissue"_id] = 55.0;
    person->risk_factors["EnergyIntake"_id] = 2000.0;
    person->risk_factors["Intercept_K"_id] = 100.0;

    EXPECT_NO_THROW(model->kevin_hall_run(*person));

    // Test income category operations (lines 717, 948)
    EXPECT_NO_THROW(model->initialise_income_category(*person, context->population()));
    EXPECT_NO_THROW(model->update_income_category(*context));
}

class TestStaticLinearModel : public ::testing::Test {
  protected:
    void SetUp() override {
        using namespace hgps;
        using namespace hgps::core;

        // Setup mock context
        context.random.seed(42); // Fixed seed for reproducibility

        // Setup test person
        person.age = 30;
        person.gender = Gender::male;
        person.region = Region::England;
        person.ethnicity = Ethnicity::White;
        person.bmi = 25.0;
        person.physical_activity = 150.0;

        // Add person to population
        population.add(person);
        context.population = &population;

        // Setup model parameters
        auto expected = std::make_shared<RiskFactorSexAgeTable>();
        auto expected_trend = std::make_shared<std::unordered_map<core::Identifier, double>>();
        auto trend_steps = std::make_shared<std::unordered_map<core::Identifier, int>>();

        auto region_models =
            std::make_shared<std::unordered_map<core::Region, LinearModelParams>>();
        (*region_models)[Region::England] = LinearModelParams{0.0, {{"age", 0.1}, {"gender", 0.2}}};
        (*region_models)[Region::Wales] = LinearModelParams{0.1, {{"age", 0.15}, {"gender", 0.25}}};

        auto ethnicity_models =
            std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>();
        (*ethnicity_models)[Ethnicity::White] =
            LinearModelParams{0.0, {{"age", 0.1}, {"gender", 0.2}, {"region", 0.3}}};
        (*ethnicity_models)[Ethnicity::Black] =
            LinearModelParams{0.1, {{"age", 0.15}, {"gender", 0.25}, {"region", 0.35}}};

        std::unordered_map<core::Income, LinearModelParams> income_models{
            {Income::Low,
             LinearModelParams{
                 0.0, {{"age", 0.1}, {"gender", 0.2}, {"region", 0.3}, {"ethnicity", 0.4}}}},
            {Income::High,
             LinearModelParams{
                 0.1, {{"age", 0.15}, {"gender", 0.25}, {"region", 0.35}, {"ethnicity", 0.45}}}}};

        model = std::make_unique<StaticLinearModel>(expected, expected_trend, trend_steps,
                                                    region_models, ethnicity_models, income_models,
                                                    0.5);
    }

    hgps::RuntimeContext context;
    hgps::Person person;
    hgps::Population population;
    std::unique_ptr<hgps::StaticLinearModel> model;
};

TEST_F(TestStaticLinearModel, BasicProperties) {
    ASSERT_EQ(RiskFactorModelType::Static, model->type());
    ASSERT_EQ("Static", model->name());
}

TEST_F(TestStaticLinearModel, InitialiseRegion) {
    model->initialise_region(context, person, context.random);

    // Verify region is assigned
    ASSERT_TRUE(person.region >= core::Region::England &&
                person.region <= core::Region::NorthernIreland);
}

TEST_F(TestStaticLinearModel, InitialiseEthnicity) {
    model->initialise_ethnicity(context, person, context.random);

    // Verify ethnicity is assigned
    ASSERT_TRUE(person.ethnicity >= core::Ethnicity::White &&
                person.ethnicity <= core::Ethnicity::Other);
}

TEST_F(TestStaticLinearModel, InitialiseIncome) {
    // Test continuous income initialization
    model->initialise_income_continuous(person, context.random);
    ASSERT_GT(person.income_continuous, -10.0); // Reasonable lower bound
    ASSERT_LT(person.income_continuous, 10.0);  // Reasonable upper bound

    // Test income category initialization
    std::vector<double> income_quantiles{0.25, 0.5, 0.75, 1.0};
    model->initialise_income_category(person, income_quantiles);
    ASSERT_TRUE(person.income_category >= 0 && person.income_category <= 3);
}

TEST_F(TestStaticLinearModel, InitialisePhysicalActivity) {
    model->initialise_physical_activity(context, person, context.random);

    // Verify physical activity is within reasonable bounds
    ASSERT_GT(person.physical_activity, 0.0);
    ASSERT_LT(person.physical_activity, 500.0); // Reasonable upper bound
}

TEST_F(TestStaticLinearModel, UpdateOperations) {
    // Test region update at age 18
    person.age = 18;
    model->update_region(context, person, context.random);
    auto initial_region = person.region;

    // Region should potentially change at age 18
    ASSERT_TRUE(person.region >= core::Region::England &&
                person.region <= core::Region::NorthernIreland);

    // Test that region doesn't change after age 18
    person.age = 19;
    model->update_region(context, person, context.random);
    ASSERT_EQ(initial_region, person.region);

    // Test income category update (every 5 years)
    auto initial_category = person.income_category;
    model->update_income_category(context);
    // Note: Can't assert exact change as it depends on population statistics
}

TEST(TestStaticLinearModel, IncomeCategoryOperations) {
    // Setup test environment
    auto context = std::make_shared<RuntimeContext>();
    Population population(10);

    // Add test persons with different incomes
    for (int i = 0; i < 10; i++) {
        Person person;
        person.income_continuous = (i + 1) * 10000.0;
        population.add(person, 2023);
    }

    context->population = &population;

    // Create model instance
    auto model = StaticLinearModel(
        std::make_shared<RiskFactorSexAgeTable>(),
        std::make_shared<std::unordered_map<core::Identifier, double>>(),
        std::make_shared<std::unordered_map<core::Identifier, int>>(),
        std::make_shared<std::unordered_map<core::Region, LinearModelParams>>(),
        std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>(),
        std::unordered_map<core::Income, LinearModelParams>(), 1.0);

    // Test income category initialization (lines 551-595)
    Person test_person;
    test_person.income_continuous = 25000.0;
    EXPECT_NO_THROW(model.initialise_income_category(test_person, population));
    EXPECT_NO_THROW(model.update_income_category(*context));
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
