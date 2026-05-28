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

hgps::input::Configuration make_quintile_stratum_configuration() {
    auto config = make_finch_configuration();
    config.modelling.baseline_adjustment.income_stratum_factors_mean.enabled = true;
    config.modelling.baseline_adjustment.income_stratum_factors_mean
        .adjustment_income_stratum_count = 5u;
    return config;
}

std::filesystem::path create_temp_weight_quantile_csv(const std::string &filename,
                                                      const std::string &content) {
    const auto dir = std::filesystem::temp_directory_path() / "hgps_weight_quantile_tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / filename;
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

nlohmann::json make_csv_file_json(const std::filesystem::path &path) {
    return nlohmann::json{{"name", path.string()},
                          {"format", "csv"},
                          {"delimiter", ","},
                          {"encoding", "ASCII"},
                          {"columns", {{"quantile", "double"}}}};
}

void seed_finch_kevin_hall_food_factors(hgps::Person &person) {
    using namespace hgps;
    static constexpr std::array<const char *, 3> k_food_keys = {"FoodCarbohydrate", "FoodProtein",
                                                                "FoodFat"};
    for (const auto *key : k_food_keys) {
        person.risk_factors[core::Identifier(key)] = 50.0;
    }
    person.risk_factors["PhysicalActivity"_id] = 1.4;
    person.risk_factors["EnergyIntake"_id] = 8000.0;
}

struct KevinHallWeightRuntime {
    std::unique_ptr<hgps::RiskFactorModelDefinition> definition_storage;
    std::unique_ptr<hgps::RiskFactorModel> model_storage;
    hgps::KevinHallModel *model{nullptr};
    hgps::RuntimeContext context;
};

KevinHallWeightRuntime make_kevin_hall_weight_runtime(const nlohmann::json &dynamic_json,
                                                      const hgps::input::Configuration &config,
                                                      std::size_t population_size,
                                                      int start_year = 2020) {
    using namespace hgps;
    using namespace hgps::core;

    auto definition_storage =
        hgps::input::load_kevinhall_risk_model_definition(dynamic_json, config);
    if (!definition_storage) {
        throw HgpsException("Failed to load Kevin Hall model definition");
    }
    auto model_storage = definition_storage->create_model();
    auto *kevin_hall = dynamic_cast<KevinHallModel *>(model_storage.get());
    if (!kevin_hall) {
        throw HgpsException("Expected KevinHallModel instance");
    }

    DataTable data;
    IntegerDataTableColumnBuilder age_builder("Age");
    age_builder.append(30);
    data.add(age_builder.build());

    const auto country =
        Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};
    const auto settings = Settings{country, 0.1f, IntegerInterval(0, 110)};
    const auto run = RunInfo{.start_time = static_cast<unsigned int>(start_year),
                             .stop_time = static_cast<unsigned int>(start_year + 5),
                             .sync_timeout_ms = 1000,
                             .seed = 42u};
    const auto ses = SESDefinition{.fuction_name = "normal", .parameters = {0.0, 1.0}};
    const auto mapping = HierarchicalMapping(
        {MappingEntry{"Weight", 0, DoubleInterval{5.0, 200.0}}, MappingEntry{"Age", 0}});
    auto project_requirements = hgps::input::ProjectRequirements{};
    project_requirements.income.type = "continuous";
    project_requirements.income.categories = "4";
    auto inputs =
        std::make_shared<ModelInput>(data, settings, run, ses, mapping, std::vector<DiseaseInfo>{},
                                     project_requirements, hgps::input::PIFInfo{});

    auto bus = std::make_shared<hgps::DefaultEventBus>();
    hgps::SyncChannel channel;
    auto scenario = std::make_unique<hgps::BaselineScenario>(channel);
    hgps::RuntimeContext context(bus, inputs, std::move(scenario));
    context.reset_population(population_size);
    context.set_current_time(start_year);
    context.set_current_run(1);

    return KevinHallWeightRuntime{.definition_storage = std::move(definition_storage),
                                  .model_storage = std::move(model_storage),
                                  .model = kevin_hall,
                                  .context = std::move(context)};
}

nlohmann::json set_quintile_weight_quantiles(nlohmann::json json,
                                             const std::filesystem::path &female_base,
                                             const std::filesystem::path &male_base,
                                             double female_scale, double male_scale) {
    auto make_quintile_block = [&](const std::filesystem::path &base, double scale) {
        nlohmann::json block;
        for (std::size_t q = 1; q <= 5; ++q) {
            const auto path = create_temp_weight_quantile_csv(
                base.filename().string() + "_q" + std::to_string(q) + ".csv",
                ",quantile\n1," + std::to_string(scale * static_cast<double>(q)) + "\n2," +
                    std::to_string(scale * static_cast<double>(q) + 0.1) + "\n3," +
                    std::to_string(scale * static_cast<double>(q) + 0.2) + "\n");
            block["Quintile" + std::to_string(q)] = make_csv_file_json(path);
        }
        return block;
    };

    json["WeightQuantiles"]["Female"] = make_quintile_block(female_base, female_scale);
    json["WeightQuantiles"]["Male"] = make_quintile_block(male_base, male_scale);
    return json;
}

} // namespace

TEST(KevinHallWeightQuantiles, LegacySingleFileConfigStillLoads) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_finch_configuration();
    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallWeightQuantiles, SingleFileWeightBroadcastLoadsWithFiveStrata) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    auto config = make_quintile_stratum_configuration();
    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallWeightQuantiles, QuintileWeightFilesLoadWhenMatchingStrataCount) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json = set_quintile_weight_quantiles(json, finch / "weight_female", finch / "weight_male", 1.0,
                                         1.0);
    auto config = make_quintile_stratum_configuration();
    auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);
    EXPECT_NE(definition->create_model(), nullptr);
}

TEST(KevinHallWeightQuantiles, QuintileWeightFileCountMismatchThrows) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json = set_quintile_weight_quantiles(json, finch / "weight_female", finch / "weight_male", 1.0,
                                         1.0);
    json["WeightQuantiles"]["Female"].erase("Quintile5");
    auto config = make_quintile_stratum_configuration();

    EXPECT_THROW(
        {
            auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
            (void)definition;
        },
        hgps::core::HgpsException);
}

TEST(KevinHallWeightQuantiles, QuintileWeightFilesRequireStratumAdjustmentEnabled) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json = set_quintile_weight_quantiles(json, finch / "weight_female", finch / "weight_male", 1.0,
                                         1.0);
    auto config = make_finch_configuration();

    EXPECT_THROW(
        {
            auto definition = hgps::input::load_kevinhall_risk_model_definition(json, config);
            (void)definition;
        },
        hgps::core::HgpsException);
}

TEST(KevinHallWeightQuantiles, GeneratePrintsWeightStratumAndIncomeCategoryTables) {
    using hgps::test::capture_stdout;

    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    constexpr int start_year = 2020;
    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json = set_quintile_weight_quantiles(json, finch / "weight_female", finch / "weight_male", 1.0,
                                         1.0);
    auto config = make_quintile_stratum_configuration();
    auto runtime = make_kevin_hall_weight_runtime(json, config, 6, start_year);

    for (std::size_t i = 0; i < runtime.context.population().size(); ++i) {
        auto &person = runtime.context.population()[i];
        person.gender = hgps::core::Gender::female;
        person.age = 12;
        person.income = hgps::core::Income::lowermiddle;
        person.has_income_adjustment_stratum = true;
        person.income_adjustment_stratum = i % 5;
        seed_finch_kevin_hall_food_factors(person);
    }

    const auto output =
        capture_stdout([&] { runtime.model->generate_risk_factors(runtime.context); });

    EXPECT_NE(output.find("[WEIGHT STRATUM ASSIGNMENT]"), std::string::npos);
    EXPECT_NE(output.find("[WEIGHT BY FINAL INCOME CATEGORY]"), std::string::npos);
    EXPECT_NE(output.find("Quintile1"), std::string::npos);
    EXPECT_NE(output.find("Quantile Size"), std::string::npos);
}

TEST(KevinHallWeightQuantiles, DifferentStrataCanProduceDifferentWeights) {
    using namespace hgps;

    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json = set_quintile_weight_quantiles(json, finch / "weight_female", finch / "weight_male", 1.0,
                                         1.0);
    auto config = make_quintile_stratum_configuration();
    auto runtime = make_kevin_hall_weight_runtime(json, config, 2, 2020);

    auto &person0 = runtime.context.population()[0];
    person0.gender = hgps::core::Gender::female;
    person0.age = 15;
    person0.has_income_adjustment_stratum = true;
    person0.income_adjustment_stratum = 0;
    seed_finch_kevin_hall_food_factors(person0);

    auto &person1 = runtime.context.population()[1];
    person1.gender = hgps::core::Gender::female;
    person1.age = 15;
    person1.has_income_adjustment_stratum = true;
    person1.income_adjustment_stratum = 4;
    seed_finch_kevin_hall_food_factors(person1);

    runtime.model->generate_risk_factors(runtime.context);

    const double weight0 = person0.risk_factors.at("Weight"_id);
    const double weight1 = person1.risk_factors.at("Weight"_id);
    EXPECT_GT(weight0, 0.0);
    EXPECT_GT(weight1, 0.0);
    EXPECT_NE(weight0, weight1);
}
