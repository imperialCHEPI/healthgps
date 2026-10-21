#include "pch.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Input/configuration.h"
#include "HealthGPS.Input/csvparser.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/jacardi_employment_static.h"
#include "HealthGPS/random_algorithm.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using namespace hgps;
using json = nlohmann::json;

namespace {

#ifndef JACARDI_EMPLOYMENT_FIXTURE_DIR
#error "JACARDI_EMPLOYMENT_FIXTURE_DIR must be set by CMake"
#endif

std::filesystem::path fixture_dir() { return std::filesystem::path{JACARDI_EMPLOYMENT_FIXTURE_DIR}; }

json file_slot(const std::string &name, const json &columns) {
    return json{{"name", name},
                {"format", "csv"},
                {"delimiter", ","},
                {"encoding", "ASCII"},
                {"columns", columns}};
}

// MAHIMA: Load partner coefs + recode the same way the parser does (direct CSV → tables).
std::shared_ptr<EmploymentStaticTables> load_fixture_tables() {
    const auto root = fixture_dir();
    auto tables = std::make_shared<EmploymentStaticTables>();
    {
        hgps::input::FileInfo coef_info;
        coef_info.name = root / "employment_static_coefs.csv";
        coef_info.format = "csv";
        coef_info.delimiter = ",";
        coef_info.columns = {{"Category", "integer"},
                             {"Factor", "string"},
                             {"Coefficient", "double"}};
        const auto table = hgps::input::load_datatable_from_csv(coef_info);
        std::unordered_map<int, std::vector<EmploymentStaticTables::FactorTerm>> by_cat;
        for (std::size_t i = 0; i < table.num_rows(); ++i) {
            const auto category = std::any_cast<int>(table.column("Category").value(i));
            const auto factor = std::any_cast<std::string>(table.column("Factor").value(i));
            const auto coefficient = std::any_cast<double>(table.column("Coefficient").value(i));
            by_cat[category].push_back(
                EmploymentStaticTables::FactorTerm{.factor = factor, .coefficient = coefficient});
        }
        tables->set_coefficients(std::move(by_cat));
    }
    {
        hgps::input::FileInfo recode_info;
        recode_info.name = root / "employment_4cat_recode.csv";
        recode_info.format = "csv";
        recode_info.delimiter = ",";
        recode_info.columns = {{"employment_id", "integer"}, {"employment4_id", "integer"}};
        const auto table = hgps::input::load_datatable_from_csv(recode_info);
        std::array<int, EmploymentStaticTables::category_count + 1> recode{};
        for (std::size_t i = 0; i < table.num_rows(); ++i) {
            const auto detailed = std::any_cast<int>(table.column("employment_id").value(i));
            const auto cat4 = std::any_cast<int>(table.column("employment4_id").value(i));
            recode[static_cast<std::size_t>(detailed)] = cat4;
        }
        tables->set_recode(recode);
    }
    return tables;
}

} // namespace

TEST(JacardiEmployment, AgeRcsAt60MatchesPartnerGolden) {
    // MAHIMA: Partner check at a=60: 17.0408, 13.4030, 9.4134, 1.4114, 0.0359
    const auto f = EmploymentStaticTables::compute_age_factors(60);
    EXPECT_NEAR(60.0, f.age, 1e-12);
    EXPECT_NEAR(0.0, f.school_fade, 1e-12);
    EXPECT_NEAR(17.0408, f.age_rcs[0], 5e-5);
    EXPECT_NEAR(13.4030, f.age_rcs[1], 5e-5);
    EXPECT_NEAR(9.4134, f.age_rcs[2], 5e-5);
    EXPECT_NEAR(1.4114, f.age_rcs[3], 5e-5);
    EXPECT_NEAR(0.0359, f.age_rcs[4], 5e-5);
}

TEST(JacardiEmployment, SchoolFadeAges19To22) {
    EXPECT_NEAR(1.0, EmploymentStaticTables::compute_age_factors(19).school_fade, 1e-12);
    EXPECT_NEAR(2.0 / 3.0, EmploymentStaticTables::compute_age_factors(20).school_fade, 1e-12);
    EXPECT_NEAR(1.0 / 3.0, EmploymentStaticTables::compute_age_factors(21).school_fade, 1e-12);
    EXPECT_NEAR(0.0, EmploymentStaticTables::compute_age_factors(22).school_fade, 1e-12);
}

TEST(JacardiEmployment, Woman19Isced2StudentProbNear0879) {
    auto tables = load_fixture_tables();
    const auto probs = tables->score_probabilities(19, core::Gender::female, 2);
    // MAHIMA: Partner example — woman 19 education_id 2 → p_5 (Student) ≈ 0.879
    EXPECT_NEAR(0.879, probs[4], 1e-3);
}

TEST(JacardiEmployment, ScoreMatchesCheckCsvSampleRows) {
    auto tables = load_fixture_tables();
    hgps::input::FileInfo check_info;
    check_info.name = fixture_dir() / "employment_static_check.csv";
    check_info.format = "csv";
    check_info.delimiter = ",";
    check_info.columns = {{"age", "integer"},
                          {"gender", "integer"},
                          {"education_id", "integer"},
                          {"p_1", "double"},
                          {"p_2", "double"},
                          {"p_3", "double"},
                          {"p_4", "double"},
                          {"p_5", "double"},
                          {"p_6", "double"}};
    const auto check = hgps::input::load_datatable_from_csv(check_info);

    // MAHIMA: Spot-check every 40th row across the golden file (full scan is fine but slower).
    const auto n = check.num_rows();
    ASSERT_GT(n, 0U);
    for (std::size_t i = 0; i < n; i += 40) {
        const auto age = std::any_cast<int>(check.column("age").value(i));
        const auto gender = std::any_cast<int>(check.column("gender").value(i));
        const auto education_id = std::any_cast<int>(check.column("education_id").value(i));
        const auto g = gender == 1 ? core::Gender::male : core::Gender::female;
        const auto probs =
            tables->score_probabilities(static_cast<unsigned int>(age), g, education_id);
        for (int c = 0; c < 6; ++c) {
            const auto col = fmt::format("p_{}", c + 1);
            const auto expected = std::any_cast<double>(check.column(col).value(i));
            EXPECT_NEAR(expected, probs[static_cast<std::size_t>(c)], 1e-6)
                << "row=" << i << " age=" << age << " gender=" << gender
                << " edu=" << education_id << " cat=" << (c + 1);
        }
    }
}

TEST(JacardiEmployment, InitialiseMissingUnder19AndRecode) {
    auto tables = load_fixture_tables();
    Random rng;

    const auto young = tables->initialise(10, core::Gender::male, 3, rng);
    EXPECT_FALSE(std::isfinite(young.detailed_id));
    EXPECT_FALSE(std::isfinite(young.cat4_id));

    const auto adult = tables->initialise(40, core::Gender::female, 3, rng);
    ASSERT_TRUE(std::isfinite(adult.detailed_id));
    ASSERT_TRUE(std::isfinite(adult.cat4_id));
    const int detailed = static_cast<int>(adult.detailed_id);
    EXPECT_EQ(tables->recode(detailed), static_cast<int>(adult.cat4_id));
}

TEST(JacardiEmployment, PartBFirstAssignThenHold) {
    auto tables = load_fixture_tables();
    Random rng;

    EmploymentStaticTables::Assignment missing{};
    const auto first = tables->update(19, core::Gender::male, 3, missing, rng);
    ASSERT_TRUE(std::isfinite(first.detailed_id));

    const auto held = tables->update(20, core::Gender::male, 3, first, rng);
    EXPECT_EQ(first.detailed_id, held.detailed_id);
    EXPECT_EQ(first.cat4_id, held.cat4_id);
}

TEST(JacardiEmployment, ParserLoadsEmploymentOnlyStatic) {
    const auto root = fixture_dir();
    json model{
        {"ModelName", "JacardiModel"},
        {"Employment",
         {{"method", "multinomial_static"},
          {"coefficients",
           file_slot("employment_static_coefs.csv", json{{"Category", "integer"},
                                                         {"Factor", "string"},
                                                         {"Coefficient", "double"}})},
          {"recode", file_slot("employment_4cat_recode.csv", json{{"employment_id", "integer"},
                                                                  {"employment4_id", "integer"}})}}}};
    hgps::input::Configuration config;
    config.root_path = root;
    auto definition = hgps::input::load_jacardi_risk_model_definition(model, config);
    ASSERT_NE(definition, nullptr);
    auto model_ptr = definition->create_model();
    ASSERT_NE(model_ptr, nullptr);
    EXPECT_EQ("JacardiModel", model_ptr->name());
}
