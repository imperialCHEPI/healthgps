#include "pch.h"

#include "HealthGPS.Core/identifier.h"
#include "HealthGPS.Input/api.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/interfaces.h"
#include "HealthGPS/kevin_hall_model.h"
#include "HealthGPS/modelinput.h"
#include "HealthGPS/runtime_context.h"
#include "TestConsoleCapture.h"

#include <gtest/gtest.h>

#include <array>
#include <fstream>

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

std::filesystem::path create_temp_height_csv(const std::string &filename,
                                             const std::string &content) {
    const auto dir = std::filesystem::temp_directory_path() / "hgps_height_tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / filename;
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

hgps::input::Configuration make_quintile_height_finch_configuration() {
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 5u;
    return config;
}

void seed_finch_kevin_hall_food_factors(hgps::Person &person) {
    using namespace hgps;
    static constexpr std::array<const char *, 21> k_food_keys = {"FoodCarbohydrate",
                                                                 "FoodProtein",
                                                                 "FoodFat",
                                                                 "FoodSodium",
                                                                 "FoodAlcohol",
                                                                 "FoodLegume",
                                                                 "FoodVegetable",
                                                                 "FoodFruit",
                                                                 "FoodProcessedMeat",
                                                                 "FoodRedMeat",
                                                                 "FoodFibre",
                                                                 "FoodTotalSugar",
                                                                 "FoodAddedSugar",
                                                                 "FoodSaturatedFat",
                                                                 "FoodPolyunsaturatedFattyAcid",
                                                                 "FoodMonounsaturatedFat",
                                                                 "FoodIron",
                                                                 "FoodCalcium",
                                                                 "FoodVitaminC",
                                                                 "FoodCopper",
                                                                 "FoodZinc"};
    for (const auto *key : k_food_keys) {
        person.risk_factors[hgps::core::Identifier(key)] = 50.0;
    }
    person.risk_factors["PhysicalActivity"_id] = 1.4;
}

std::shared_ptr<hgps::ModelInput>
make_kevin_hall_height_model_input(hgps::core::DataTable &data, int start_year,
                                   std::string income_categories = "4") {
    using namespace hgps;
    using namespace hgps::core;

    IntegerDataTableColumnBuilder age_builder("Age");
    age_builder.append(0);
    data.add(age_builder.build());

    const auto country =
        Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};
    const auto settings = Settings{country, 0.1f, IntegerInterval(0, 110)};
    const auto run = RunInfo{.start_time = static_cast<unsigned int>(start_year),
                             .stop_time = static_cast<unsigned int>(start_year + 5),
                             .sync_timeout_ms = 1000,
                             .seed = 42u};
    const auto ses = SESDefinition{.fuction_name = "normal", .parameters = {0.0, 1.0}};
    // Allow newborn weights from initialise_weight (often below 15 kg).
    const auto mapping = HierarchicalMapping(
        {MappingEntry{"Weight", 0, DoubleInterval{5.0, 200.0}}, MappingEntry{"Age", 0}});
    auto project_requirements = hgps::input::ProjectRequirements{};
    project_requirements.income.type = "continuous";
    project_requirements.income.categories = std::move(income_categories);
    return std::make_shared<ModelInput>(data, settings, run, ses, mapping,
                                        std::vector<DiseaseInfo>{}, project_requirements,
                                        hgps::input::PIFInfo{});
}

struct KevinHallHeightRuntime {
    // KevinHallModel holds references to nutrient/food maps owned by the definition.
    std::unique_ptr<hgps::RiskFactorModelDefinition> definition_storage;
    std::unique_ptr<hgps::RiskFactorModel> model_storage;
    hgps::KevinHallModel *model{nullptr};
    hgps::RuntimeContext context;
};

KevinHallHeightRuntime make_kevin_hall_height_runtime(const nlohmann::json &dynamic_json,
                                                      const hgps::input::Configuration &config,
                                                      std::size_t population_size, int start_year,
                                                      std::string income_categories = "4") {
    auto definition_storage =
        hgps::input::load_kevinhall_risk_model_definition(dynamic_json, config);
    if (!definition_storage) {
        throw hgps::core::HgpsException("Failed to load Kevin Hall model definition");
    }
    auto model_storage = definition_storage->create_model();
    auto *kevin_hall = dynamic_cast<hgps::KevinHallModel *>(model_storage.get());
    if (!kevin_hall) {
        throw hgps::core::HgpsException("Expected KevinHallModel instance");
    }

    hgps::core::DataTable data;
    auto inputs =
        make_kevin_hall_height_model_input(data, start_year, std::move(income_categories));
    auto bus = std::make_shared<hgps::DefaultEventBus>();
    hgps::SyncChannel channel;
    auto scenario = std::make_unique<hgps::BaselineScenario>(channel);
    hgps::RuntimeContext context(bus, inputs, std::move(scenario));
    context.reset_population(population_size);
    context.set_current_time(start_year);
    context.set_current_run(1);

    return KevinHallHeightRuntime{.definition_storage = std::move(definition_storage),
                                  .model_storage = std::move(model_storage),
                                  .model = kevin_hall,
                                  .context = std::move(context)};
}

} // namespace

TEST(KevinHallHeight, LegacyHeightScalarConfigStillLoads) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    // WeightQuantiles in FINCH is often configured as Quintile1..N, which requires the income
    // stratum adjustment feature to be enabled. This test is about legacy Height scalar loading,
    // so switch WeightQuantiles to the legacy single-file form to avoid coupling.
    json["WeightQuantiles"]["Female"] = {{"name", "weight_quantiles_NCDRisk_female.csv"},
                                         {"format", "csv"},
                                         {"delimiter", ","},
                                         {"encoding", "ASCII"},
                                         {"columns", {{"quantile", "double"}}}};
    json["WeightQuantiles"]["Male"] = {{"name", "weight_quantiles_NCDRisk_male.csv"},
                                       {"format", "csv"},
                                       {"delimiter", ","},
                                       {"encoding", "ASCII"},
                                       {"columns", {{"quantile", "double"}}}};
    json.erase("Height");
    json["HeightSlope"] = {{"Female", 0.123}, {"Male", 0.127}};
    json["HeightStdDev"] = {{"Female", 0.041}, {"Male", 0.043}};
    auto config = make_finch_configuration();

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, SingleRowHeightBroadcastLoadsWithFiveStrata) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto female_csv = create_temp_height_csv("height_female_single.csv", "slope,std\n0.18,0.05\n");
    auto male_csv = create_temp_height_csv("height_male_single.csv", "slope,std\n0.20,0.06\n");

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 5u;

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, MultiRowHeightLoadsWhenMatchingStrataCount) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    const std::string rows =
        ",slope,std\nq1,0.11,0.041\nq2,0.12,0.042\nq3,0.13,0.043\nq4,0.14,0.044\nq5,0.15,0.045\n";
    auto female_csv = create_temp_height_csv("height_female_multi.csv", rows);
    auto male_csv = create_temp_height_csv("height_male_multi.csv", rows);

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 5u;

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, MultiRowHeightThrowsWhenRowCountMismatchesStrataCount) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    const std::string rows = "slope,std\n0.11,0.041\n0.12,0.042\n0.13,0.043\n";
    auto female_csv = create_temp_height_csv("height_female_invalid.csv", rows);
    auto male_csv = create_temp_height_csv("height_male_invalid.csv", rows);

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 5u;

    EXPECT_THROW(
        {
            auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
            (void)definition;
        },
        hgps::core::HgpsException);
}

TEST(KevinHallHeight, KeyedThreeColumnCsvLoads) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    const std::string rows = ",slope,std\nincome_quintile1,0.11,0.041\nincome_quintile2,0.12,0."
                             "042\nincome_quintile3,0.13,"
                             "0.043\nincome_quintile4,0.14,0.044\nincome_quintile5,0.15,0.045\n";
    auto female_csv = create_temp_height_csv("height_female_keyed.csv", rows);
    auto male_csv = create_temp_height_csv("height_male_keyed.csv", rows);

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 5u;

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, InvalidFourColumnHeightCsvThrows) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    const std::string rows = "key,slope,std,extra\nq1,0.1,0.2,0.3\n";
    auto female_csv = create_temp_height_csv("height_female_badcols.csv", rows);
    auto male_csv = create_temp_height_csv("height_male_badcols.csv", rows);

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();

    EXPECT_THROW(
        {
            auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
            (void)definition;
        },
        hgps::core::HgpsException);
}

TEST(KevinHallHeight, FemaleAndMaleHeightCanUseDifferentValidShapes) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto female_csv =
        create_temp_height_csv("height_female_broadcast.csv", "slope,std\n0.17,0.051\n");
    const std::string male_rows =
        "slope,std\n0.20,0.041\n0.21,0.042\n0.22,0.043\n0.23,0.044\n0.24,0.045\n";
    auto male_csv = create_temp_height_csv("height_male_strata.csv", male_rows);

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 5u;

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, MultiRowHeightLoadsWhenStratumAdjustmentDisabled) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    const std::string rows = "slope,std\n0.11,0.041\n0.12,0.042\n0.13,0.043\n";
    auto female_csv = create_temp_height_csv("height_female_nostratum.csv", rows);
    auto male_csv = create_temp_height_csv("height_male_nostratum.csv", rows);

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    // This test only targets Height CSV parsing when stratum adjustment is disabled. FINCH
    // WeightQuantiles is configured as Quintile1..N, which requires stratum adjustment; switch it
    // to the legacy single-file form so the test remains focused.
    json["WeightQuantiles"]["Female"] = {{"name", "weight_quantiles_NCDRisk_female.csv"},
                                         {"format", "csv"},
                                         {"delimiter", ","},
                                         {"encoding", "ASCII"},
                                         {"columns", {{"quantile", "double"}}}};
    json["WeightQuantiles"]["Male"] = {{"name", "weight_quantiles_NCDRisk_male.csv"},
                                       {"format", "csv"},
                                       {"delimiter", ","},
                                       {"encoding", "ASCII"},
                                       {"columns", {{"quantile", "double"}}}};
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = false;

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, HeaderOnlyHeightCsvThrows) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto female_csv = create_temp_height_csv("height_female_header_only.csv", "slope,std\n");
    auto male_csv = create_temp_height_csv("height_male_header_only.csv", "slope,std\n");

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();

    EXPECT_THROW(
        {
            auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
            (void)definition;
        },
        hgps::core::HgpsException);
}

TEST(KevinHallHeight, SingleRowHeightLoadsWhenStratumCountIsOne) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto female_csv =
        create_temp_height_csv("height_female_one_stratum.csv", "slope,std\n0.18,0.05\n");
    auto male_csv = create_temp_height_csv("height_male_one_stratum.csv", "slope,std\n0.20,0.06\n");

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    // With adjustment_income_stratum_count == 1, WeightQuantiles cannot use Quintile1..N files.
    // Use legacy single-file quantiles so this test remains focused on Height CSV broadcasting.
    json["WeightQuantiles"]["Female"] = {{"name", "weight_quantiles_NCDRisk_female.csv"},
                                         {"format", "csv"},
                                         {"delimiter", ","},
                                         {"encoding", "ASCII"},
                                         {"columns", {{"quantile", "double"}}}};
    json["WeightQuantiles"]["Male"] = {{"name", "weight_quantiles_NCDRisk_male.csv"},
                                       {"format", "csv"},
                                       {"delimiter", ","},
                                       {"encoding", "ASCII"},
                                       {"columns", {{"quantile", "double"}}}};
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 1u;

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, MultiRowHeightThrowsWhenStratumCountIsOne) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    const std::string rows = "slope,std\n0.11,0.041\n0.12,0.042\n";
    auto female_csv = create_temp_height_csv("height_female_bad_one_stratum.csv", rows);
    auto male_csv = create_temp_height_csv("height_male_bad_one_stratum.csv", rows);

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 1u;

    EXPECT_THROW(
        {
            auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
            (void)definition;
        },
        hgps::core::HgpsException);
}

TEST(KevinHallHeight, HeightCsvSupportsSemicolonDelimiter) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto female_csv =
        create_temp_height_csv("height_female_semicolon.csv", "slope;std\n0.18;0.05\n");
    auto male_csv = create_temp_height_csv("height_male_semicolon.csv", "slope;std\n0.20;0.06\n");

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    // This test is about Height delimiter parsing; decouple from WeightQuantiles stratum settings.
    json["WeightQuantiles"]["Female"] = {{"name", "weight_quantiles_NCDRisk_female.csv"},
                                         {"format", "csv"},
                                         {"delimiter", ","},
                                         {"encoding", "ASCII"},
                                         {"columns", {{"quantile", "double"}}}};
    json["WeightQuantiles"]["Male"] = {{"name", "weight_quantiles_NCDRisk_male.csv"},
                                       {"format", "csv"},
                                       {"delimiter", ","},
                                       {"encoding", "ASCII"},
                                       {"columns", {{"quantile", "double"}}}};
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    json["Height"]["Female"]["delimiter"] = ";";
    json["Height"]["Male"]["delimiter"] = ";";
    auto config = make_finch_configuration();

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, HeightCsvWithoutHeaderRowStillLoads) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto female_csv = create_temp_height_csv("height_female_no_header.csv", "0.18,0.05\n");
    auto male_csv = create_temp_height_csv("height_male_no_header.csv", "0.20,0.06\n");

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    json["Height"]["Male"]["name"] = male_csv.string();
    auto config = make_quintile_height_finch_configuration();

    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallHeight, HeightCsvWrongColumnCountThrows) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto female_csv = create_temp_height_csv("height_female_one_col.csv", "0.18\n");

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json["Height"]["Female"]["name"] = female_csv.string();
    auto config = make_finch_configuration();

    EXPECT_THROW(
        {
            auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
            (void)definition;
        },
        hgps::core::HgpsException);
}

TEST(KevinHallHeight, GenerateInitialisesHeightWithQuintileParams) {
    using namespace hgps;
    using hgps::test::capture_stdout;
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    constexpr int start_year = 2020;
    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_quintile_height_finch_configuration();
    auto runtime = make_kevin_hall_height_runtime(json, config, 5, start_year);

    const std::array<hgps::core::Income, 5> incomes = {
        hgps::core::Income::low, hgps::core::Income::lowermiddle, hgps::core::Income::middle,
        hgps::core::Income::uppermiddle, hgps::core::Income::high};
    for (std::size_t i = 0; i < runtime.context.population().size(); ++i) {
        auto &person = runtime.context.population()[i];
        person.gender = (i % 2 == 0) ? hgps::core::Gender::female : hgps::core::Gender::male;
        person.age = 10;
        person.income = incomes[i];
        person.has_income_adjustment_stratum = true;
        person.income_adjustment_stratum = i;
        seed_finch_kevin_hall_food_factors(person);
    }

    ASSERT_NO_THROW(capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); }));

    for (const auto &person : runtime.context.population()) {
        ASSERT_TRUE(person.is_active());
        EXPECT_TRUE(person.risk_factors.contains("Height"_id));
        EXPECT_TRUE(person.risk_factors.contains("Height_residual"_id));
        EXPECT_GT(person.risk_factors.at("Height"_id), 0.0);
    }
}

TEST(KevinHallHeight, GeneratePrintsHeightStratumAndIncomeCategoryTables) {
    using hgps::test::capture_stdout;

    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    constexpr int start_year = 2020;
    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_quintile_height_finch_configuration();
    auto runtime = make_kevin_hall_height_runtime(json, config, 6, start_year);

    for (std::size_t i = 0; i < runtime.context.population().size(); ++i) {
        auto &person = runtime.context.population()[i];
        person.gender = (i % 2 == 0) ? hgps::core::Gender::female : hgps::core::Gender::male;
        person.age = 12;
        person.income = hgps::core::Income::lowermiddle;
        person.has_income_adjustment_stratum = true;
        person.income_adjustment_stratum = i % 5;
        seed_finch_kevin_hall_food_factors(person);
    }

    const auto output =
        capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); });

    EXPECT_NE(output.find("[HEIGHT STRATUM ASSIGNMENT]"), std::string::npos);
    EXPECT_NE(output.find("[HEIGHT BY FINAL INCOME CATEGORY]"), std::string::npos);
    EXPECT_NE(output.find("Quintile1"), std::string::npos);
    EXPECT_NE(output.find("LowerMid"), std::string::npos);
}

TEST(KevinHallHeight, GenerateHeightSummarySkippedAfterFirstUpdateYear) {
    using hgps::test::capture_stdout;

    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    constexpr int start_year = 2020;
    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_quintile_height_finch_configuration();
    auto runtime = make_kevin_hall_height_runtime(json, config, 2, start_year);

    for (auto &person : runtime.context.population()) {
        person.gender = hgps::core::Gender::female;
        person.age = 11;
        person.income = hgps::core::Income::middle;
        person.has_income_adjustment_stratum = true;
        person.income_adjustment_stratum = 2;
        seed_finch_kevin_hall_food_factors(person);
    }

    capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); });
    std::fflush(stdout);
    runtime.context.set_current_time(start_year + 2);

    const auto output =
        capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); });
    std::fflush(stdout);

    EXPECT_EQ(output.find("[HEIGHT STRATUM ASSIGNMENT]"), std::string::npos);
    EXPECT_EQ(output.find("[HEIGHT BY FINAL INCOME CATEGORY]"), std::string::npos);
}

TEST(KevinHallHeight, UpdateChildrenRefreshesHeightForSubAdultAges) {
    using namespace hgps;
    using hgps::test::capture_stdout;
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    constexpr int start_year = 2020;
    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_quintile_height_finch_configuration();
    auto runtime = make_kevin_hall_height_runtime(json, config, 2, start_year);

    for (std::size_t i = 0; i < runtime.context.population().size(); ++i) {
        auto &person = runtime.context.population()[i];
        person.gender = hgps::core::Gender::male;
        person.age = static_cast<unsigned>(8 + i);
        person.income = hgps::core::Income::high;
        person.has_income_adjustment_stratum = true;
        person.income_adjustment_stratum = i;
        seed_finch_kevin_hall_food_factors(person);
    }

    // Keep this test focused on update semantics (height refresh for sub-adults) rather than
    // console reporting. Summary tables are only printed at start_year and start_year+1.
    // Running outside that window avoids very verbose output and potential test-runner stalls.
    runtime.context.set_current_time(start_year + 2);
    capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); });
    auto &person = runtime.context.population()[0];
    const double height_before = person.risk_factors.at("Height"_id);

    // update_non_newborns re-runs initialise_weight for children, which resets Weight.
    // Height_residual is preserved and update_height uses it for sub-adult ages.
    person.risk_factors["Height_residual"_id] += 0.05;

    runtime.context.set_current_time(start_year + 3);
    ASSERT_NO_THROW(capture_stdout([&] { runtime.model->update_risk_factors(runtime.context); }));

    const double height_after = person.risk_factors.at("Height"_id);
    EXPECT_NE(height_before, height_after);
}

TEST(KevinHallHeight, UpdateNewbornsInitialisesHeight) {
    using namespace hgps;
    using hgps::test::capture_stdout;

    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    constexpr int start_year = 2020;
    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_quintile_height_finch_configuration();
    auto runtime = make_kevin_hall_height_runtime(json, config, 1, start_year);

    auto &person = runtime.context.population()[0];
    person.gender = hgps::core::Gender::female;
    person.age = 0;
    person.income = hgps::core::Income::low;
    person.has_income_adjustment_stratum = true;
    person.income_adjustment_stratum = 0;
    seed_finch_kevin_hall_food_factors(person);

    capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); });
    person.risk_factors.erase("Height"_id);
    person.risk_factors.erase("Height_residual"_id);

    runtime.context.set_current_time(start_year + 1);
    const auto output =
        capture_stdout([&] { runtime.model->update_risk_factors(runtime.context); });

    EXPECT_TRUE(person.risk_factors.contains("Height"_id));
    EXPECT_TRUE(person.risk_factors.contains("Height_residual"_id));
    EXPECT_NE(output.find("[HEIGHT STRATUM ASSIGNMENT]"), std::string::npos);
}

TEST(KevinHallHeight, GeneratePrintsHeightTablesForThreeIncomeCategories) {
    using hgps::test::capture_stdout;

    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    constexpr int start_year = 2020;
    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_quintile_height_finch_configuration();
    auto runtime =
        make_kevin_hall_height_runtime(json, config, 3, start_year, /*income_categories=*/"3");

    const std::array<hgps::core::Income, 3> incomes = {
        hgps::core::Income::low, hgps::core::Income::middle, hgps::core::Income::high};
    for (std::size_t i = 0; i < runtime.context.population().size(); ++i) {
        auto &person = runtime.context.population()[i];
        person.gender = hgps::core::Gender::female;
        person.age = 10;
        person.income = incomes[i];
        person.has_income_adjustment_stratum = true;
        person.income_adjustment_stratum = i;
        seed_finch_kevin_hall_food_factors(person);
    }

    const auto output =
        capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); });

    EXPECT_NE(output.find("[HEIGHT BY FINAL INCOME CATEGORY]"), std::string::npos);
    EXPECT_NE(output.find("Middle"), std::string::npos);
    EXPECT_EQ(output.find("LowerMid"), std::string::npos);
}

TEST(KevinHallHeight, GenerateUsesBroadcastHeightWhenStratumNotAssigned) {
    using namespace hgps;
    using hgps::test::capture_stdout;
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    constexpr int start_year = 2020;
    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_quintile_height_finch_configuration();
    auto runtime = make_kevin_hall_height_runtime(json, config, 2, start_year);

    for (auto &person : runtime.context.population()) {
        person.gender = hgps::core::Gender::female;
        person.age = 10;
        person.income = hgps::core::Income::middle;
        person.has_income_adjustment_stratum = false;
        person.income_adjustment_stratum = 4;
        seed_finch_kevin_hall_food_factors(person);
    }

    capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); });
    EXPECT_GT(runtime.context.population()[0].risk_factors.at("Height"_id), 0.0);
}
