#include "pch.h"

#include "HealthGPS.Core/api.h"
#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/identifier.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/modelinput.h"
#include "HealthGPS/risk_factor_adjustable_model.h"
#include "HealthGPS/runtime_context.h"
#include "HealthGPS/static_linear_model.h"

#include <algorithm>
#include <fstream>

namespace {
struct TempCsvPair {
    std::filesystem::path male;
    std::filesystem::path female;
};

class TestAdjustableModel final : public hgps::RiskFactorAdjustableModel {
  public:
    using hgps::RiskFactorAdjustableModel::RiskFactorAdjustableModel;

    hgps::RiskFactorModelType type() const noexcept override {
        return hgps::RiskFactorModelType::Static;
    }

    std::string name() const noexcept override { return "TestAdjustableModel"; }

    void generate_risk_factors(hgps::RuntimeContext & /*context*/) override {}

    void update_risk_factors(hgps::RuntimeContext & /*context*/) override {}
};

std::shared_ptr<hgps::ModelInput> create_minimal_model_input(hgps::core::DataTable &data) {
    using namespace hgps;
    using namespace hgps::core;

    IntegerDataTableColumnBuilder age_builder("Age");
    age_builder.append(0);
    data.add(age_builder.build());

    auto country = Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};
    auto settings = Settings{country, 0.1f, IntegerInterval(0, 2)};
    auto run = RunInfo{.start_time = 2022,
                       .stop_time = 2023,
                       .sync_timeout_ms = 1000,
                       .seed = 123u,
                       .verbosity = core::VerboseMode::none,
                       .comorbidities = 0,
                       .policy_start_year = 0};
    auto ses = SESDefinition{.fuction_name = "normal", .parameters = {0.0, 1.0}};
    auto mapping = HierarchicalMapping({MappingEntry{"Age", 0}});
    std::vector<core::DiseaseInfo> diseases{};
    auto project_requirements = hgps::input::ProjectRequirements{};
    project_requirements.risk_factors.adjust_to_factors_mean = true;
    project_requirements.risk_factors.trended = false;
    project_requirements.income.enabled = true;
    project_requirements.income.type = "continuous";
    project_requirements.income.categories = "4";
    project_requirements.income.adjust_to_factors_mean = true;
    project_requirements.income.trended = false;
    project_requirements.physical_activity.enabled = false;
    project_requirements.physical_activity.type = "simple";
    project_requirements.physical_activity.adjust_to_factors_mean = true;
    project_requirements.physical_activity.trended = false;
    project_requirements.trend.enabled = false;
    return std::make_shared<ModelInput>(data, settings, run, ses, mapping, diseases,
                                        project_requirements, hgps::input::PIFInfo{});
}

std::shared_ptr<hgps::ModelInput> create_feature_off_model_input(hgps::core::DataTable &data) {
    using namespace hgps;
    using namespace hgps::core;

    IntegerDataTableColumnBuilder age_builder("Age");
    age_builder.append(0);
    data.add(age_builder.build());

    auto country = Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};
    auto settings = Settings{country, 0.1f, IntegerInterval(0, 2)};
    auto run = RunInfo{.start_time = 2022,
                       .stop_time = 2024,
                       .sync_timeout_ms = 1000,
                       .seed = 123u,
                       .verbosity = core::VerboseMode::none,
                       .comorbidities = 0,
                       .policy_start_year = 0};
    auto ses = SESDefinition{.fuction_name = "normal", .parameters = {0.0, 1.0}};
    auto mapping = HierarchicalMapping({MappingEntry{"Age", 0}});
    std::vector<core::DiseaseInfo> diseases{};
    auto project_requirements = hgps::input::ProjectRequirements{};
    project_requirements.risk_factors.adjust_to_factors_mean = true;
    project_requirements.risk_factors.trended = false;
    project_requirements.income.enabled = true;
    project_requirements.income.type = "continuous";
    project_requirements.income.categories = "4";
    project_requirements.income.adjust_to_factors_mean = true;
    project_requirements.income.trended = false;
    project_requirements.physical_activity.enabled = false;
    project_requirements.physical_activity.type = "simple";
    project_requirements.physical_activity.adjust_to_factors_mean = true;
    project_requirements.physical_activity.trended = false;
    project_requirements.trend.enabled = false;
    return std::make_shared<ModelInput>(data, settings, run, ses, mapping, diseases,
                                        project_requirements, hgps::input::PIFInfo{});
}

std::shared_ptr<hgps::ModelInput> create_trended_model_input(hgps::core::DataTable &data) {
    using namespace hgps;
    using namespace hgps::core;

    IntegerDataTableColumnBuilder age_builder("Age");
    age_builder.append(0);
    data.add(age_builder.build());

    auto country = Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};
    auto settings = Settings{country, 0.1f, IntegerInterval(0, 2)};
    auto run = RunInfo{.start_time = 2022,
                       .stop_time = 2024,
                       .sync_timeout_ms = 1000,
                       .seed = 123u,
                       .verbosity = core::VerboseMode::none,
                       .comorbidities = 0,
                       .policy_start_year = 0};
    auto ses = SESDefinition{.fuction_name = "normal", .parameters = {0.0, 1.0}};
    auto mapping = HierarchicalMapping({MappingEntry{"Age", 0}});
    std::vector<core::DiseaseInfo> diseases{};
    auto project_requirements = hgps::input::ProjectRequirements{};
    project_requirements.risk_factors.adjust_to_factors_mean = true;
    project_requirements.risk_factors.trended = true;
    project_requirements.income.enabled = true;
    project_requirements.income.type = "continuous";
    project_requirements.income.categories = "4";
    project_requirements.income.adjust_to_factors_mean = true;
    project_requirements.income.trended = true;
    project_requirements.physical_activity.enabled = false;
    project_requirements.physical_activity.type = "simple";
    project_requirements.physical_activity.adjust_to_factors_mean = true;
    project_requirements.physical_activity.trended = true;
    project_requirements.trend.enabled = true;
    return std::make_shared<ModelInput>(data, settings, run, ses, mapping, diseases,
                                        project_requirements, hgps::input::PIFInfo{});
}

std::shared_ptr<hgps::RiskFactorSexAgeTable>
make_expected_table(const hgps::core::Identifier &factor, double value) {
    auto expected = std::make_shared<hgps::RiskFactorSexAgeTable>();
    expected->emplace(hgps::core::Gender::male, factor, std::vector<double>{value, value, value});
    expected->emplace(hgps::core::Gender::female, factor, std::vector<double>{value, value, value});
    return expected;
}

std::shared_ptr<hgps::RiskFactorSexAgeTable>
make_expected_table_with_income_and_pa(const hgps::core::Identifier &factor, double factor_value,
                                       double income_value, double pa_value) {
    auto expected = std::make_shared<hgps::RiskFactorSexAgeTable>();
    const std::vector<double> factor_row{factor_value, factor_value, factor_value};
    const std::vector<double> income_row{income_value, income_value, income_value};
    const std::vector<double> pa_row{pa_value, pa_value, pa_value};
    expected->emplace(hgps::core::Gender::male, factor, factor_row);
    expected->emplace(hgps::core::Gender::female, factor, factor_row);
    expected->emplace(hgps::core::Gender::male, hgps::core::Identifier("income"), income_row);
    expected->emplace(hgps::core::Gender::female, hgps::core::Identifier("income"), income_row);
    expected->emplace(hgps::core::Gender::male, hgps::core::Identifier("PhysicalActivity"), pa_row);
    expected->emplace(hgps::core::Gender::female, hgps::core::Identifier("PhysicalActivity"),
                      pa_row);
    return expected;
}

TempCsvPair write_temp_expected_csv_pair(std::string_view suffix, std::size_t age_rows) {
    const auto base = std::filesystem::temp_directory_path() /
                      ("healthgps_income_stratum_" + std::string(suffix) + "_" +
                       std::to_string(static_cast<unsigned long long>(std::rand())));
    std::filesystem::create_directories(base);
    const auto male = base / "male.csv";
    const auto female = base / "female.csv";

    auto write_file = [&](const std::filesystem::path &path) {
        std::ofstream out(path.string());
        out << "age,income\n";
        for (std::size_t i = 0; i < age_rows; ++i) {
            out << i << "," << (500.0 + static_cast<double>(i)) << '\n';
        }
    };
    write_file(male);
    write_file(female);
    return TempCsvPair{.male = male, .female = female};
}

hgps::input::Configuration make_config_for_expected_load(const std::filesystem::path &male_file,
                                                         const std::filesystem::path &female_file,
                                                         int max_age) {
    hgps::input::Configuration config{};
    config.settings.age_range = hgps::core::IntegerInterval(0, max_age);
    config.modelling.baseline_adjustment.format = "CSV";
    config.modelling.baseline_adjustment.delimiter = ",";
    config.modelling.baseline_adjustment.file_names["factorsmean_male"] = male_file;
    config.modelling.baseline_adjustment.file_names["factorsmean_female"] = female_file;
    return config;
}

struct StaticLinearModelTestBundle {
    std::vector<hgps::core::Identifier> names;
    std::vector<hgps::LinearModelParams> models;
    std::vector<hgps::core::DoubleInterval> ranges;
    std::vector<double> lambda;
    std::vector<double> stddev;
    Eigen::MatrixXd cholesky;
    std::vector<hgps::LinearModelParams> policy_models;
    std::vector<hgps::core::DoubleInterval> policy_ranges;
    Eigen::MatrixXd policy_cholesky;
    std::shared_ptr<std::vector<hgps::LinearModelParams>> trend_models;
    std::shared_ptr<std::vector<hgps::core::DoubleInterval>> trend_ranges;
    std::shared_ptr<std::vector<double>> trend_lambda;
    std::unordered_map<hgps::core::Identifier, std::unordered_map<hgps::core::Gender, double>>
        rural_prevalence;
    std::unordered_map<hgps::core::Income, hgps::LinearModelParams> income_models;
    hgps::LinearModelParams continuous_income_model;
    std::vector<hgps::LinearModelParams> logistic_models;
    std::shared_ptr<std::unordered_map<hgps::core::Identifier, double>> expected_trend;
    std::shared_ptr<std::unordered_map<hgps::core::Identifier, int>> trend_steps;
    std::shared_ptr<std::unordered_map<hgps::core::Identifier, double>> expected_trend_boxcox;
    std::unique_ptr<hgps::StaticLinearModel> model;
};

std::shared_ptr<StaticLinearModelTestBundle> create_test_static_linear_model_bundle(
    const std::shared_ptr<hgps::RiskFactorSexAgeTable> &overall_expected,
    const std::vector<hgps::IncomeStratumExpectedTableEntry> &stratum_tables, bool stratum_enabled,
    std::size_t adjustment_income_stratum_count) {
    using namespace hgps;
    auto bundle = std::make_shared<StaticLinearModelTestBundle>();
    bundle->names = {core::Identifier("foodcarbohydrate")};
    bundle->models = {LinearModelParams{.intercept = 95.0}};
    bundle->ranges = {core::DoubleInterval(0.0, 500.0)};
    bundle->lambda = {1.0};
    bundle->stddev = {0.0};
    bundle->cholesky = Eigen::MatrixXd::Identity(1, 1);
    bundle->policy_models = {LinearModelParams{}};
    bundle->policy_ranges = {core::DoubleInterval(0.0, 500.0)};
    bundle->policy_cholesky = Eigen::MatrixXd::Identity(1, 1);
    bundle->trend_models = std::make_shared<std::vector<LinearModelParams>>(1, LinearModelParams{});
    bundle->trend_ranges =
        std::make_shared<std::vector<core::DoubleInterval>>(1, core::DoubleInterval(0.0, 500.0));
    bundle->trend_lambda = std::make_shared<std::vector<double>>(1, 1.0);
    bundle->rural_prevalence = {
        {"Under18"_id, {{core::Gender::male, 0.2}, {core::Gender::female, 0.2}}},
        {"Over18"_id, {{core::Gender::male, 0.2}, {core::Gender::female, 0.2}}}};
    bundle->income_models = {{core::Income::low, LinearModelParams{}},
                             {core::Income::lowermiddle, LinearModelParams{}},
                             {core::Income::uppermiddle, LinearModelParams{}},
                             {core::Income::high, LinearModelParams{}}};
    bundle->continuous_income_model = LinearModelParams{.intercept = 650.0};
    bundle->logistic_models = {LinearModelParams{}};
    bundle->expected_trend = std::make_shared<std::unordered_map<core::Identifier, double>>();
    bundle->trend_steps = std::make_shared<std::unordered_map<core::Identifier, int>>();
    bundle->expected_trend_boxcox =
        std::make_shared<std::unordered_map<core::Identifier, double>>();
    bundle->model = nullptr;

    // MAHIMA: StaticLinearModel stores many constructor inputs by reference.
    // Heap-own the bundle so reference members remain valid after helper returns.
    const std::unordered_map<core::Identifier, PhysicalActivityModel> physical_activity_models{};
    bundle->model = std::make_unique<StaticLinearModel>(
        overall_expected, bundle->expected_trend, bundle->trend_steps,
        bundle->expected_trend_boxcox, bundle->names, bundle->models, bundle->ranges,
        bundle->lambda, bundle->stddev, bundle->cholesky, bundle->policy_models,
        bundle->policy_ranges, bundle->policy_cholesky, bundle->trend_models, bundle->trend_ranges,
        bundle->trend_lambda, 0.0, bundle->rural_prevalence, bundle->income_models, 0.01,
        TrendType::Null, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true,
        bundle->continuous_income_model, "4", physical_activity_models, stratum_tables,
        stratum_enabled, adjustment_income_stratum_count, false, bundle->logistic_models);
    return bundle;
}
} // namespace

TEST(IncomeStratumAdjustment, CapturesSingleDebugRowForFilteredStratumAdjustment) {
    using namespace hgps;

    core::DataTable data;
    auto inputs = create_minimal_model_input(data);
    auto bus = std::make_shared<DefaultEventBus>();
    auto channel = SyncChannel{};
    auto scenario = std::make_unique<BaselineScenario>(channel);
    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    context.reset_population(2);

    auto factor = core::Identifier("foodcarbohydrate");
    context.population()[0].gender = core::Gender::male;
    context.population()[1].gender = core::Gender::female;
    context.population()[0].age = 0;
    context.population()[1].age = 0;
    context.population()[0].has_income_adjustment_stratum = true;
    context.population()[1].has_income_adjustment_stratum = true;
    context.population()[0].income_adjustment_stratum = 0;
    context.population()[1].income_adjustment_stratum = 0;
    context.population()[0].risk_factors[factor] = 10.0;
    context.population()[1].risk_factors[factor] = 10.0;

    auto expected = make_expected_table(factor, 20.0);
    auto expected_trend = std::make_shared<std::unordered_map<core::Identifier, double>>();
    auto trend_steps = std::make_shared<std::unordered_map<core::Identifier, int>>();
    auto model = TestAdjustableModel(expected, expected_trend, trend_steps, TrendType::Null,
                                     nullptr, nullptr);

    std::vector<IncomeStratumAdjustmentExampleRow> rows;
    model.adjust_risk_factors(context, std::vector<core::Identifier>{factor}, std::nullopt, false,
                              expected.get(), 0u, &rows);

    EXPECT_NEAR(20.0, context.population()[0].risk_factors.at(factor), 1e-9);
    EXPECT_NEAR(20.0, context.population()[1].risk_factors.at(factor), 1e-9);

    ASSERT_FALSE(rows.empty());
    const auto it = std::find_if(rows.begin(), rows.end(), [](const auto &row) {
        return row.factor == "foodcarbohydrate" && row.age == 0;
    });
    ASSERT_NE(rows.end(), it);
    const auto &row = *it;
    EXPECT_EQ(0u, row.bucket);
    EXPECT_NEAR(20.0, row.expected_value, 1e-9);
    EXPECT_NEAR(10.0, row.simulated_mean, 1e-9);
    EXPECT_NEAR(10.0, row.delta, 1e-9);
    EXPECT_NEAR(10.0, row.current_value, 1e-9);
    EXPECT_NEAR(20.0, row.after_delta_value, 1e-9);
    EXPECT_NEAR(20.0, row.final_value, 1e-9);
    EXPECT_TRUE(row.person_id == context.population()[0].id() ||
                row.person_id == context.population()[1].id());
}

TEST(IncomeStratumAdjustment, StaticLinearModelAppliesStrataInGenerateAndUpdate) {
    using namespace hgps;

    core::DataTable data;
    auto inputs = create_minimal_model_input(data);
    auto bus = std::make_shared<DefaultEventBus>();
    auto channel = SyncChannel{};
    auto scenario = std::make_unique<BaselineScenario>(channel);
    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    context.reset_population(8);

    for (std::size_t i = 0; i < context.population().size(); ++i) {
        auto &p = context.population()[i];
        p.gender = (i % 2 == 0) ? core::Gender::male : core::Gender::female;
        p.age = 0;
    }

    const auto factor = core::Identifier("foodcarbohydrate");
    auto overall_expected = make_expected_table_with_income_and_pa(factor, 100.0, 600.0, 1.8);
    auto q1_expected = make_expected_table_with_income_and_pa(factor, 90.0, 600.0, 1.6);
    auto q2_expected = make_expected_table_with_income_and_pa(factor, 110.0, 600.0, 2.0);

    std::vector<IncomeStratumExpectedTableEntry> stratum_tables{
        {"Quintile1", q1_expected},
        {"Quintile2", q2_expected},
    };
    auto bundle =
        create_test_static_linear_model_bundle(overall_expected, stratum_tables, true, 2u);

    bundle->model->generate_risk_factors(context);
    for (const auto &p : context.population()) {
        if (!p.is_active()) {
            continue;
        }
        EXPECT_TRUE(p.has_income_adjustment_stratum);
        EXPECT_LT(p.income_adjustment_stratum, 2u);
        EXPECT_NE(core::Income::unknown, p.income);
    }

    context.set_current_time(2023);
    bundle->model->update_risk_factors(context);
    for (const auto &p : context.population()) {
        if (!p.is_active()) {
            continue;
        }
        EXPECT_TRUE(p.has_income_adjustment_stratum);
        EXPECT_LT(p.income_adjustment_stratum, 2u);
        EXPECT_NE(core::Income::unknown, p.income);
    }
}

TEST(IncomeStratumAdjustment, FeatureOffKeepsLegacyPathWithoutStratumAssignment) {
    using namespace hgps;

    core::DataTable data;
    auto inputs = create_feature_off_model_input(data);
    auto bus = std::make_shared<DefaultEventBus>();
    auto channel = SyncChannel{};
    auto scenario = std::make_unique<BaselineScenario>(channel);
    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    context.reset_population(8);
    for (std::size_t i = 0; i < context.population().size(); ++i) {
        auto &p = context.population()[i];
        p.gender = (i % 2 == 0) ? core::Gender::male : core::Gender::female;
        p.age = 0;
    }

    const auto factor = core::Identifier("foodcarbohydrate");
    auto overall_expected = make_expected_table_with_income_and_pa(factor, 100.0, 600.0, 1.8);
    auto q1_expected = make_expected_table_with_income_and_pa(factor, 90.0, 600.0, 1.6);
    auto q2_expected = make_expected_table_with_income_and_pa(factor, 110.0, 600.0, 2.0);
    std::vector<IncomeStratumExpectedTableEntry> stratum_tables{
        {"Quintile1", q1_expected},
        {"Quintile2", q2_expected},
    };
    auto bundle =
        create_test_static_linear_model_bundle(overall_expected, stratum_tables, false, 2u);

    // MAHIMA: Regression guard for "feature off" path. Even if tables are present in memory,
    // runtime should remain on legacy behaviour and must not stamp adjustment-stratum fields.
    bundle->model->generate_risk_factors(context);
    for (const auto &p : context.population()) {
        if (!p.is_active()) {
            continue;
        }
        EXPECT_FALSE(p.has_income_adjustment_stratum);
        EXPECT_EQ(0u, p.income_adjustment_stratum);
        EXPECT_NE(core::Income::unknown, p.income);
    }

    context.set_current_time(2023);
    bundle->model->update_risk_factors(context);
    for (const auto &p : context.population()) {
        if (!p.is_active()) {
            continue;
        }
        EXPECT_FALSE(p.has_income_adjustment_stratum);
        EXPECT_EQ(0u, p.income_adjustment_stratum);
        EXPECT_NE(core::Income::unknown, p.income);
    }
}

TEST(IncomeStratumAdjustment, UpdateYearRecomputesStrataAfterStateReset) {
    using namespace hgps;

    core::DataTable data;
    auto inputs = create_minimal_model_input(data);
    auto bus = std::make_shared<DefaultEventBus>();
    auto channel = SyncChannel{};
    auto scenario = std::make_unique<BaselineScenario>(channel);
    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    context.reset_population(8);
    for (std::size_t i = 0; i < context.population().size(); ++i) {
        auto &p = context.population()[i];
        p.gender = (i % 2 == 0) ? core::Gender::male : core::Gender::female;
        p.age = 0;
    }

    const auto factor = core::Identifier("foodcarbohydrate");
    auto overall_expected = make_expected_table_with_income_and_pa(factor, 100.0, 600.0, 1.8);
    auto q1_expected = make_expected_table_with_income_and_pa(factor, 90.0, 600.0, 1.6);
    auto q2_expected = make_expected_table_with_income_and_pa(factor, 110.0, 600.0, 2.0);
    std::vector<IncomeStratumExpectedTableEntry> stratum_tables{
        {"Quintile1", q1_expected},
        {"Quintile2", q2_expected},
    };
    auto bundle =
        create_test_static_linear_model_bundle(overall_expected, stratum_tables, true, 2u);

    bundle->model->generate_risk_factors(context);

    // MAHIMA: Yearly-path check. We explicitly wipe per-person stratum flags to verify update()
    // always recomputes assignment from current continuous income each year.
    for (auto &p : context.population()) {
        if (!p.is_active()) {
            continue;
        }
        p.has_income_adjustment_stratum = false;
        p.income_adjustment_stratum = 0u;
    }

    context.set_current_time(2023);
    bundle->model->update_risk_factors(context);
    for (const auto &p : context.population()) {
        if (!p.is_active()) {
            continue;
        }
        EXPECT_TRUE(p.has_income_adjustment_stratum);
        EXPECT_LT(p.income_adjustment_stratum, 2u);
        EXPECT_NE(core::Income::unknown, p.income);
    }
}

TEST(IncomeStratumAdjustment, LoadExpectedFailsWhenCsvAgeCoverageTooShort) {
    using namespace hgps;

    const auto files = write_temp_expected_csv_pair("short_coverage", 2u);
    auto config = make_config_for_expected_load(files.male, files.female, 2);

    // MAHIMA: Phase-3 parser/load failure coverage. The loader must fail fast when expected CSV
    // age coverage does not reach the simulation age upper bound.
    EXPECT_THROW((void)input::load_risk_factor_expected(config), core::HgpsException);

    std::filesystem::remove(files.male);
    std::filesystem::remove(files.female);
    std::filesystem::remove(files.male.parent_path());
}

TEST(IncomeStratumAdjustment, LoadExpectedFailsWhenCsvFilesMissing) {
    using namespace hgps;

    const auto base = std::filesystem::temp_directory_path() /
                      ("healthgps_income_stratum_missing_" +
                       std::to_string(static_cast<unsigned long long>(std::rand())));
    auto config =
        make_config_for_expected_load(base / "missing_male.csv", base / "missing_female.csv", 2);

    // MAHIMA: Phase-3 parser/load failure coverage for missing files.
    // We assert fail-fast behavior so invalid baseline-adjustment paths are reported immediately.
    EXPECT_THROW((void)input::load_risk_factor_expected(config), core::HgpsException);
}

TEST(IncomeStratumAdjustment, TrendedYearlyPathKeepsStrataAndFinalIncomeAssigned) {
    using namespace hgps;

    core::DataTable data;
    auto inputs = create_trended_model_input(data);
    auto bus = std::make_shared<DefaultEventBus>();
    auto channel = SyncChannel{};
    auto scenario = std::make_unique<BaselineScenario>(channel);
    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    context.reset_population(10);
    for (std::size_t i = 0; i < context.population().size(); ++i) {
        auto &p = context.population()[i];
        p.gender = (i % 2 == 0) ? core::Gender::male : core::Gender::female;
        p.age = 0;
    }

    const auto factor = core::Identifier("foodcarbohydrate");
    auto overall_expected = make_expected_table_with_income_and_pa(factor, 100.0, 600.0, 1.8);
    auto q1_expected = make_expected_table_with_income_and_pa(factor, 90.0, 600.0, 1.6);
    auto q2_expected = make_expected_table_with_income_and_pa(factor, 110.0, 600.0, 2.0);
    std::vector<IncomeStratumExpectedTableEntry> stratum_tables{
        {"Quintile1", q1_expected},
        {"Quintile2", q2_expected},
    };
    auto bundle =
        create_test_static_linear_model_bundle(overall_expected, stratum_tables, true, 2u);

    bundle->model->generate_risk_factors(context);

    // MAHIMA: Broader yearly-path guard.
    // With trend.enabled + *.trended set true, update() should execute the trended branch and still
    // maintain valid per-person strata and final income-category assignment every year.
    context.set_current_time(2023);
    bundle->model->update_risk_factors(context);
    for (const auto &p : context.population()) {
        if (!p.is_active()) {
            continue;
        }
        EXPECT_TRUE(p.has_income_adjustment_stratum);
        EXPECT_LT(p.income_adjustment_stratum, 2u);
        EXPECT_NE(core::Income::unknown, p.income);
    }
}
