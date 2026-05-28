#include "pch.h"

#include "HealthGPS.Input/api.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/kevin_hall_model.h"
#include "HealthGPS/modelinput.h"
#include "HealthGPS/runtime_context.h"

#include <gtest/gtest.h>

namespace {

std::filesystem::path healthgps_repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::filesystem::path finch_data_root() {
    return healthgps_repo_root() / "input-data/data/KevinHall_FINCH";
}

hgps::input::Configuration make_finch_configuration() {
    const auto finch = finch_data_root();
    hgps::input::Configuration config;
    config.root_path = finch;
    config.settings.age_range = hgps::core::IntegerInterval(0, 110);
    config.modelling.baseline_adjustment.format = "CSV";
    config.modelling.baseline_adjustment.delimiter = ",";
    config.modelling.baseline_adjustment.file_names["factorsmean_male"] =
        finch / "Finch.FactorsMean.Male.csv";
    config.modelling.baseline_adjustment.file_names["factorsmean_female"] =
        finch / "Finch.FactorsMean.Female.csv";
    config.project_requirements.income.type = "continuous";
    config.project_requirements.income.categories = "4";
    return config;
}

std::shared_ptr<hgps::ModelInput> make_weight_validation_input(hgps::core::DataTable &data) {
    using namespace hgps;
    using namespace hgps::core;

    IntegerDataTableColumnBuilder age_builder("Age");
    age_builder.append(30);
    data.add(age_builder.build());

    const auto country =
        Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};
    const auto settings = Settings{country, 0.1f, IntegerInterval(0, 110)};
    const auto run = RunInfo{.start_time = 2020, .stop_time = 2025, .seed = 1u};
    const auto ses = SESDefinition{.fuction_name = "normal", .parameters = {0.0, 1.0}};
    const auto mapping = HierarchicalMapping(
        {MappingEntry{"Weight", 0, DoubleInterval{40.0, 120.0}}, MappingEntry{"Age", 0}});

    return std::make_shared<ModelInput>(data, settings, run, ses, mapping,
                                        std::vector<DiseaseInfo>{},
                                        hgps::input::ProjectRequirements{}, hgps::input::PIFInfo{});
}

} // namespace

TEST(KevinHallWeightValidation, ValidateWeightThrowsWhenOutsideConfiguredRange) {
    using namespace hgps;
    using namespace hgps::core;

    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    const auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_finch_configuration();
    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);

    auto model = definition->create_model();
    auto *kevin_hall = dynamic_cast<KevinHallModel *>(model.get());
    ASSERT_NE(kevin_hall, nullptr);

    DataTable data;
    auto bus = std::make_shared<DefaultEventBus>();
    SyncChannel channel;
    auto inputs = make_weight_validation_input(data);
    auto scenario = std::make_unique<BaselineScenario>(channel);
    RuntimeContext context(bus, inputs, std::move(scenario));
    context.set_current_time(2022);
    context.set_current_run(1);

    Person person;
    person.set_id(42);
    person.age = 35;
    person.gender = Gender::female;
    person.region = "region1";
    person.ethnicity = "ethnicity1";
    person.income = Income::low;
    person.risk_factors["Weight"_id] = 100.0;
    person.risk_factors["Height"_id] = 165.0;
    person.risk_factors["BMI"_id] = 28.0;
    person.risk_factors["EnergyIntake"_id] = 2000.0;

    EXPECT_NO_THROW(kevin_hall->validate_weight_in_config_range(context, person, "unit_test"));

    person.risk_factors["Weight"_id] = 150.0;
    try {
        kevin_hall->validate_weight_in_config_range(context, person, "unit_test");
        FAIL() << "Expected weight validation to throw for weight above range";
    } catch (const HgpsException &ex) {
        const std::string message = ex.what();
        EXPECT_NE(message.find("above maximum"), std::string::npos);
        EXPECT_NE(message.find("person_id=42"), std::string::npos);
        EXPECT_NE(message.find("unit_test"), std::string::npos);
    }

    person.risk_factors["Weight"_id] = 200.0;
    try {
        kevin_hall->validate_weight_in_config_range(context, person, "unit_test");
        FAIL() << "Expected weight validation to throw";
    } catch (const HgpsException &ex) {
        const std::string message = ex.what();
        EXPECT_NE(message.find("above maximum"), std::string::npos);
        EXPECT_NE(message.find("person_id=42"), std::string::npos);
        EXPECT_NE(message.find("unit_test"), std::string::npos);
    }

    person.risk_factors["Weight"_id] = 30.0;
    try {
        kevin_hall->validate_weight_in_config_range(context, person, "below_min");
        FAIL() << "Expected weight validation to throw";
    } catch (const HgpsException &ex) {
        EXPECT_NE(std::string(ex.what()).find("below minimum"), std::string::npos);
    }

    person.risk_factors.erase("Weight"_id);
    EXPECT_NO_THROW(kevin_hall->validate_weight_in_config_range(context, person, "no_weight"));
}

TEST(KevinHallWeightValidation, GeneratePrintsHeightSummaryTablesForBaselineStartYear) {
    using namespace hgps;
    using namespace hgps::core;

    auto expected = std::make_shared<RiskFactorSexAgeTable>();
    for (const auto gender : {Gender::female, Gender::male}) {
        expected->emplace(gender, "FoodCarbohydrate"_id, std::vector<double>{120.0});
        expected->emplace(gender, "FoodSodium"_id, std::vector<double>{2.0});
        expected->emplace(gender, "Weight"_id, std::vector<double>{65.0});
        expected->emplace(gender, "EnergyIntake"_id, std::vector<double>{1800.0});
        expected->emplace(gender, "PhysicalActivity"_id, std::vector<double>{1.3});
        expected->emplace(gender, "Height"_id, std::vector<double>{165.0});
    }
    auto expected_trend = std::make_shared<std::unordered_map<Identifier, double>>();
    auto trend_steps = std::make_shared<std::unordered_map<Identifier, int>>();

    const std::unordered_map<Identifier, double> energy_equation{{"Carbohydrate"_id, 1.0},
                                                                  {"Sodium"_id, 1.0}};
    const std::unordered_map<Identifier, DoubleInterval> nutrient_ranges{
        {"Carbohydrate"_id, DoubleInterval{0.0, 5000.0}},
        {"Sodium"_id, DoubleInterval{0.0, 5000.0}},
    };
    const std::unordered_map<Identifier, std::map<Identifier, double>> nutrient_equations{
        {"FoodCarbohydrate"_id, {{"Carbohydrate"_id, 1.0}}},
        {"FoodSodium"_id, {{"Sodium"_id, 1.0}}},
    };
    const std::unordered_map<Identifier, std::optional<double>> food_prices{
        {"FoodCarbohydrate"_id, std::nullopt},
        {"FoodSodium"_id, std::nullopt},
    };
    const std::unordered_map<Gender, std::vector<double>> weight_quantiles{
        {Gender::female, {1.0}},
        {Gender::male, {1.0}},
    };
    const std::vector<double> epa_quantiles{1.0};
    const std::unordered_map<Gender, std::vector<HeightModelParams>> height_params{
        {Gender::female, {HeightModelParams{.slope = 0.18, .stddev = 0.05}}},
        {Gender::male, {HeightModelParams{.slope = 0.22, .stddev = 0.05}}},
    };

    KevinHallModel model(expected, expected_trend, trend_steps, energy_equation, nutrient_ranges,
                         nutrient_equations, food_prices, weight_quantiles, epa_quantiles,
                         height_params);

    DataTable data;
    IntegerDataTableColumnBuilder age_builder("Age");
    age_builder.append(0);
    data.add(age_builder.build());
    const auto country =
        Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};
    const auto settings = Settings{country, 0.1f, IntegerInterval(0, 110)};
    const auto run = RunInfo{.start_time = 2020, .stop_time = 2025, .seed = 1u};
    const auto ses = SESDefinition{.fuction_name = "normal", .parameters = {0.0, 1.0}};
    const auto mapping = HierarchicalMapping(
        {MappingEntry{"Weight", 0, DoubleInterval{0.0, 250.0}}, MappingEntry{"Age", 0}});
    auto inputs = std::make_shared<ModelInput>(data, settings, run, ses, mapping,
                                               std::vector<DiseaseInfo>{},
                                               hgps::input::ProjectRequirements{},
                                               hgps::input::PIFInfo{});

    auto bus = std::make_shared<DefaultEventBus>();
    SyncChannel channel;
    auto scenario = std::make_unique<BaselineScenario>(channel);
    RuntimeContext context(bus, inputs, std::move(scenario));
    context.reset_population(2);
    context.set_current_time(context.start_time());

    for (std::size_t i = 0; i < context.population().size(); ++i) {
        auto &person = context.population()[i];
        person.gender = (i % 2 == 0) ? Gender::female : Gender::male;
        person.age = 0;
        person.income = Income::low;
        person.has_income_adjustment_stratum = true;
        person.income_adjustment_stratum = 0;
        person.risk_factors["FoodCarbohydrate"_id] = 120.0;
        person.risk_factors["FoodSodium"_id] = 2.0;
        person.risk_factors["PhysicalActivity"_id] = 1.3;
    }

    EXPECT_NO_THROW(model.generate_risk_factors(context));
    for (const auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        EXPECT_TRUE(person.risk_factors.contains("Height"_id));
    }
}

