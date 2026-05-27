#include "pch.h"

#include "HealthGPS.Input/api.h"
#include "HealthGPS.Input/model_parser.h"

#include <gtest/gtest.h>

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

} // namespace

TEST(KevinHallHeight, LegacyHeightScalarConfigStillLoads) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "dynamic_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    auto json = hgps::input::load_json(finch / "dynamic_model.json");
    json.erase("Height");
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
