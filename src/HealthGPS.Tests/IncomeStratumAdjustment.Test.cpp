#include "pch.h"

#include "HealthGPS.Core/api.h"
#include "HealthGPS.Core/identifier.h"
#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/modelinput.h"
#include "HealthGPS/risk_factor_adjustable_model.h"
#include "HealthGPS/runtime_context.h"

namespace {
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
    return std::make_shared<ModelInput>(data, settings, run, ses, mapping, diseases,
                                        project_requirements, hgps::input::PIFInfo{});
}

std::shared_ptr<hgps::RiskFactorSexAgeTable> make_expected_table(const hgps::core::Identifier &factor,
                                                                  double value) {
    auto expected = std::make_shared<hgps::RiskFactorSexAgeTable>();
    expected->emplace(hgps::core::Gender::male, factor, std::vector<double>{value, value, value});
    expected->emplace(hgps::core::Gender::female, factor, std::vector<double>{value, value, value});
    return expected;
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
    auto model =
        TestAdjustableModel(expected, expected_trend, trend_steps, TrendType::Null, nullptr, nullptr);

    std::vector<IncomeStratumAdjustmentExampleRow> rows;
    model.adjust_risk_factors(context, std::vector<core::Identifier>{factor}, std::nullopt, false,
                              expected.get(), 0u, &rows);

    EXPECT_NEAR(20.0, context.population()[0].risk_factors.at(factor), 1e-9);
    EXPECT_NEAR(20.0, context.population()[1].risk_factors.at(factor), 1e-9);

    ASSERT_EQ(1u, rows.size());
    const auto &row = rows.front();
    EXPECT_EQ(0u, row.bucket);
    EXPECT_EQ("foodcarbohydrate", row.factor);
    EXPECT_EQ(0, row.age);
    EXPECT_NEAR(20.0, row.expected_value, 1e-9);
    EXPECT_NEAR(10.0, row.simulated_mean, 1e-9);
    EXPECT_NEAR(10.0, row.delta, 1e-9);
    EXPECT_NEAR(10.0, row.current_value, 1e-9);
    EXPECT_NEAR(20.0, row.final_value, 1e-9);
    EXPECT_TRUE(row.person_id == context.population()[0].id() || row.person_id == context.population()[1].id());
}
