#include "pch.h"

#include "TestConsoleCapture.h"
#include "data_config.h"

#include "HealthGPS.Input/api.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/static_linear_model.h"

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
    // Overall factors-mean files in FINCH only contain anthropometrics; quintile files include
    // nutrients.
    config.modelling.baseline_adjustment.file_names["factorsmean_male"] =
        finch / "Finch.FactorsMean.Male.Quintile1.csv";
    config.modelling.baseline_adjustment.file_names["factorsmean_female"] =
        finch / "Finch.FactorsMean.Female.Quintile1.csv";
    config.project_requirements.income.type = "continuous";
    config.project_requirements.income.categories = "4";
    config.project_requirements.physical_activity.enabled = true;
    config.project_requirements.physical_activity.type = "continuous";
    config.project_requirements.trend.type = "null";
    config.modelling.risk_factor_models["static"] = finch / "static_model.json";
    return config;
}

} // namespace

TEST(ModelParserFinch, LoadsStaticLinearDefinitionFromFinchData) {
    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "static_model.json")) {
        GTEST_SKIP() << "FINCH input data not available at " << finch.string();
    }

    const auto json = hgps::input::load_json(finch / "static_model.json");
    auto config = make_finch_configuration();

    auto definition = hgps::input::load_staticlinear_risk_model_definition(json, config);
    ASSERT_NE(definition, nullptr);

    auto model = definition->create_model();
    ASSERT_NE(model, nullptr);
    EXPECT_TRUE(dynamic_cast<hgps::StaticLinearModel *>(model.get())->is_continuous_income_model());
}

TEST(ModelParserFinch, RegisterModelsPrintsStaticLinearSummaryBox) {
    using hgps::test::capture_stdout;

    const auto finch = finch_data_root();
    if (!std::filesystem::exists(finch / "static_model.json")) {
        GTEST_SKIP() << "FINCH input data not available";
    }

    auto config = make_finch_configuration();
    config.project_requirements.demographics.region = true;
    config.project_requirements.demographics.ethnicity = true;

    hgps::input::DataManager manager(test_datastore_path);
    hgps::CachedRepository repository(manager);

    const auto output = capture_stdout(
        [&] { hgps::input::register_risk_factor_model_definitions(repository, config); });

    EXPECT_NE(output.find("Static linear model configuration"), std::string::npos);
    EXPECT_NE(output.find("Income"), std::string::npos);
    EXPECT_NE(output.find("income_model.csv"), std::string::npos);
    EXPECT_NE(output.find("Demographics (assignment CSVs)"), std::string::npos);
}
