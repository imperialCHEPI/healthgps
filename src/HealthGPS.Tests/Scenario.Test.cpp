#include "pch.h"

#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/simple_policy_scenario.h"
#include "HealthGPS/marketing_dynamic_scenario.h"
#include "HealthGPS/fiscal_scenario.h"
#include "HealthGPS/random_algorithm.h"
#include "HealthGPS/mtrandom.h"

inline const hgps::core::Identifier bmi_key = hgps::core::Identifier{ "bmi" };
inline const hgps::core::Identifier energy_key = hgps::core::Identifier{ "energy" };

hgps::FiscalPolicyDefinition create_fiscal_policy_definition(hgps::FiscalImpactType impact_type)
{
    using namespace hgps;
    auto period = PolicyInterval{ 2022, 2030 };
    auto impacts = std::vector<PolicyImpact>{
        PolicyImpact{energy_key, -0.01, 5, 9},
        PolicyImpact{energy_key, -0.02, 10, 17},
        PolicyImpact{energy_key, -0.002, 18}
    };

    return FiscalPolicyDefinition{ impact_type, period, impacts };
}

hgps::MarketingDynamicDefinition create_dynamic_marketing_definition(std::vector<double> dynamic)
{
    using namespace hgps;
    auto period = PolicyInterval(2022, 2030);
    auto impacts = std::vector<PolicyImpact>{
        PolicyImpact{bmi_key, -0.12, 5, 12},
        PolicyImpact{bmi_key, -0.31, 13, 18},
        PolicyImpact{bmi_key, -0.16, 19}
    };

    return MarketingDynamicDefinition{ period, impacts, PolicyDynamic{dynamic} };
}

TEST(ScenarioTest, BaselineDefaultConstruction)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto scenario = BaselineScenario{ channel };

    ASSERT_EQ(ScenarioType::baseline, scenario.type());
    ASSERT_EQ("Baseline", scenario.name());
    ASSERT_EQ(0, scenario.channel().size());
}

TEST(ScenarioTest, BaselineApplyLoopback)
{
    using namespace hgps;
    using namespace hgps::core;

    auto channel = SyncChannel{};
    auto scenario = BaselineScenario{ channel };
    auto engine = MTRandom32{123456789};
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);

    auto factor_values = std::vector<double>{ 13.66, 7.13, 3.14, 105.0, 365.5 };
    for (auto& value : factor_values) {
        auto impact_value = scenario.apply(generator, entity, 2010, bmi_key, value);
        ASSERT_EQ(value, impact_value);
    }
}

TEST(ScenarioTest, PolicyPeriodOpenConstruction)
{
    using namespace hgps;
    auto period = PolicyInterval(2022);

    ASSERT_EQ(2022, period.start_time);
    ASSERT_FALSE(period.finish_time.has_value());
    ASSERT_FALSE(period.contains(2021));
    ASSERT_TRUE(period.contains(2022));
    ASSERT_TRUE(period.contains(2030));
}

TEST(ScenarioTest, PolicyPeriodClosedConstruction)
{
    using namespace hgps;

    auto period = PolicyInterval(2022, 2030);
    ASSERT_EQ(2022, period.start_time);
    ASSERT_TRUE(period.finish_time.has_value());
    ASSERT_FALSE(period.contains(2021));
    ASSERT_TRUE(period.contains(2022));
    ASSERT_TRUE(period.contains(2025));
    ASSERT_TRUE(period.contains(2030));
    ASSERT_FALSE(period.contains(2031));
}

TEST(ScenarioTest, PolicyPeriodNegativeStartThrows)
{
    using namespace hgps;
    ASSERT_THROW(PolicyInterval(-2022, 2021), std::out_of_range);
}

TEST(ScenarioTest, PolicyPeriodInvertedThrows)
{
    using namespace hgps;
    ASSERT_THROW(PolicyInterval(2022, 2021), std::out_of_range);
}

TEST(ScenarioTest, PolicyPeriodInvertedPeriodThrows)
{
    using namespace hgps;
    ASSERT_THROW(PolicyInterval(2023, 2022), std::out_of_range);
}

TEST(ScenarioTest, InterventionConstruction)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{bmi_key, 0.02, 0 } };
    auto period = PolicyInterval(2022);
    auto definition = SimplePolicyDefinition{ impact_type, risk_factor, period };
    auto scenario = SimplePolicyScenario{ channel, std::move(definition) };

    ASSERT_EQ(ScenarioType::intervention, scenario.type());
    ASSERT_EQ("Intervention", scenario.name());
    ASSERT_EQ(impact_type, scenario.impact_type());
    ASSERT_EQ(period.start_time, scenario.active_period().start_time);
    ASSERT_EQ(period.finish_time, scenario.active_period().finish_time);
    ASSERT_EQ(1, scenario.impacts().size());
    ASSERT_EQ(0, scenario.channel().size());
}

TEST(ScenarioTest, InterventionApplyAbsolute)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{bmi_key, 0.02 ,0} };
    auto other_factor_key = core::Identifier{ "xyz" };
    auto period = PolicyInterval(2021, 2030);
    auto scenario = SimplePolicyScenario{ channel,
        SimplePolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    auto impact = scenario.apply(generator, entity, 2023, bmi_key, value);
    auto no_impact = scenario.apply(generator, entity, 2023, other_factor_key, value);
    ASSERT_GT(impact, value);
    ASSERT_EQ(100.02, impact);
    ASSERT_EQ(value, no_impact);
}

TEST(ScenarioTest, InterventionApplyRelative)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);
    auto impact_type = PolicyImpactType::relative;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{bmi_key, 0.02, 0 } };
    auto other_factor_key = core::Identifier{ "xyz" };
    auto period = PolicyInterval(2021, 2030);
    auto scenario = SimplePolicyScenario{ channel,
    SimplePolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    auto impact = scenario.apply(generator, entity, 2025, bmi_key, value);
    auto no_impact = scenario.apply(generator, entity, 2025, other_factor_key, value);
    ASSERT_GT(impact, value);
    ASSERT_EQ(102.0, impact);
    ASSERT_EQ(value, no_impact);
}

TEST(ScenarioTest, InterventionApplyOutsidePeriod)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{bmi_key, 0.02, 0} };
    auto other_factor_key = core::Identifier{ "xyz" };
    auto period = PolicyInterval(2021, 2030);
    auto scenario = SimplePolicyScenario{ channel,
    SimplePolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    ASSERT_EQ(value, scenario.apply(generator, entity, 2025,other_factor_key, value));
    ASSERT_EQ(value, scenario.apply(generator, entity, 2010, bmi_key, value));
    ASSERT_EQ(value, scenario.apply(generator, entity, 2020, bmi_key, value));
    ASSERT_EQ(value, scenario.apply(generator, entity, 2031, bmi_key, value));
    ASSERT_EQ(value, scenario.apply(generator, entity, 2050, bmi_key, value));
}

TEST(ScenarioTest, InterventionApplyOpenPeriod)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{bmi_key, 0.02, 0 } };
    auto period = PolicyInterval(2021);
    auto scenario = SimplePolicyScenario{ channel,
    SimplePolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    auto expected = 100.02;
    ASSERT_EQ(value, scenario.apply(generator, entity, 2020, bmi_key, value));
    ASSERT_EQ(expected, scenario.apply(generator, entity, 2021, bmi_key, value));
    ASSERT_EQ(expected, scenario.apply(generator, entity, 2025, bmi_key, value));
}

TEST(ScenarioTest, InterventionApplyMultiple)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);
    auto impact_type = PolicyImpactType::absolute;
    auto other_factor_key = core::Identifier{ "xyz" };
    auto alcohol_factor_key = core::Identifier{ "alcohol" };

    auto risk_factor = std::vector<PolicyImpact>{
        PolicyImpact{bmi_key, 0.02, 0},
        PolicyImpact{alcohol_factor_key, 0.015, 0}
    };

    auto period = PolicyInterval(2021, 2030);
    auto scenario = SimplePolicyScenario{ channel,
        SimplePolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    auto bmi_impact = scenario.apply(generator, entity, 2023, bmi_key, value);
    auto beer_impact = scenario.apply(generator, entity, 2023, alcohol_factor_key, value);
    auto no_impact = scenario.apply(generator, entity, 2023, other_factor_key, value);

    ASSERT_GT(bmi_impact, value);
    ASSERT_GT(beer_impact, value);
    ASSERT_GT(bmi_impact, beer_impact);
    ASSERT_EQ(100.02, bmi_impact);
    ASSERT_EQ(100.015, beer_impact);
    ASSERT_EQ(value, no_impact);
}


TEST(ScenarioTest, FiscalPolicyDefinitionCreate)
{
    using namespace hgps;

    auto low_policy = create_fiscal_policy_definition(FiscalImpactType::pessimist);
    auto medium_policy = create_fiscal_policy_definition(FiscalImpactType::optimist);

    ASSERT_EQ(FiscalImpactType::pessimist, low_policy.impact_type);
    ASSERT_EQ(FiscalImpactType::optimist, medium_policy.impact_type);
}

TEST(ScenarioTest, FiscalPolicyLowImpactNone)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);
    entity.age = 3;

    auto factor_value = 100.0;
    auto delta_value = 10.0;
    auto expected = delta_value;

    entity.risk_factors.emplace(energy_key, factor_value);
    auto policy = FiscalPolicyScenario{ channel, create_fiscal_policy_definition(FiscalImpactType::pessimist) };

    auto policy_delta = policy.apply(generator, entity, 2022, energy_key, delta_value);
    ASSERT_EQ(ScenarioType::intervention, policy.type());
    ASSERT_EQ("Intervention", policy.name());
    ASSERT_EQ(expected, policy_delta);
}

TEST(ScenarioTest, FiscalPolicyLowImpactClear)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);

    auto factor_value = 100.0;
    auto delta_value = 10.0;
    auto ages = std::vector{ 3, 8, 13, 20 };
    auto expected = std::vector{10.0, 9.0, 8.0, 9.8 };

    entity.risk_factors.emplace(energy_key, factor_value);
    auto policy = FiscalPolicyScenario{ channel, create_fiscal_policy_definition(FiscalImpactType::pessimist) };

    ASSERT_EQ(ScenarioType::intervention, policy.type());
    for (size_t i = 0; i < ages.size(); i++) {
        entity.age = ages.at(i);
        auto policy_delta = policy.apply(generator, entity, 2022, energy_key, delta_value);
        ASSERT_EQ(expected.at(i), policy_delta);
        policy.clear();
    }
}

TEST(ScenarioTest, FiscalPolicyLowImpactWalk)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);

    auto factor_value = 100.0;
    auto delta_value = 10.0;
    auto ages = std::vector{ 3, 8, 13, 20 };
    auto expected = std::vector{ 10.0, 9.0, 9.0, 11.8 };

    entity.risk_factors.emplace(energy_key, factor_value);
    auto policy = FiscalPolicyScenario{ channel, create_fiscal_policy_definition(FiscalImpactType::pessimist) };

    ASSERT_EQ(ScenarioType::intervention, policy.type());
    for (size_t i = 0; i < ages.size(); i++) {
        entity.age = ages.at(i);
        auto policy_delta = policy.apply(generator, entity, 2022, energy_key, delta_value);
        ASSERT_EQ(expected.at(i), policy_delta);
    }
}

TEST(ScenarioTest, FiscalPolicyMediumImpactClear)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);

    auto factor_value = 100.0;
    auto delta_value = 10.0;
    auto ages = std::vector{ 3, 8, 13, 20 };
    auto expected = std::vector{ 10.0, 9.0, 8.0, 9.8 };

    entity.risk_factors.emplace(energy_key, factor_value);
    auto policy = FiscalPolicyScenario{ channel, create_fiscal_policy_definition(FiscalImpactType::optimist) };

    ASSERT_EQ(ScenarioType::intervention, policy.type());
    for (size_t i = 0; i < ages.size(); i++) {
        entity.age = ages.at(i);
        auto policy_delta = policy.apply(generator, entity, 2022, energy_key, delta_value);
        ASSERT_EQ(expected.at(i), policy_delta);
        policy.clear();
    }
}

TEST(ScenarioTest, FiscalPolicyMediumImpactWalk)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);

    auto factor_value = 100.0;
    auto delta_value = 10.0;
    auto ages = std::vector{ 3, 8, 13, 20 };
    auto expected = std::vector{ 10.0, 9.0, 9.0, 10.0 };

    entity.risk_factors.emplace(energy_key, factor_value);
    auto policy = FiscalPolicyScenario{ channel, create_fiscal_policy_definition(FiscalImpactType::optimist) };

    ASSERT_EQ(ScenarioType::intervention, policy.type());
    for (size_t i = 0; i < ages.size(); i++) {
        entity.age = ages.at(i);
        auto policy_delta = policy.apply(generator, entity, 2022, energy_key, delta_value);
        ASSERT_EQ(expected.at(i), policy_delta);
    }
}

TEST(ScenarioTest, MarketingPolicyCreate)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto engine = MTRandom32{ 123456789 };
    auto generator = Random{ engine };
    auto entity = Person(core::Gender::male);

    auto factor_key = core::Identifier{ "bmi" };
    auto dynamic = std::vector{ 1.0, 0.0, 0.0 };

    auto factor_value = 25.0;
    auto ages = std::vector{ 3, 8, 13, 20, 25, 30 };
    auto expected = std::vector{ 25.0, 24.88, 24.81, 25.15, 25.0, 25.0 };

    entity.risk_factors.emplace(factor_key, factor_value);
    auto policy = MarketingDynamicScenario{ channel, create_dynamic_marketing_definition(dynamic) };

    ASSERT_EQ(ScenarioType::intervention, policy.type());
    ASSERT_EQ("Intervention", policy.name());
    for (size_t i = 0; i < ages.size(); i++) {
        entity.age = ages.at(i);
        auto policy_delta = policy.apply(generator, entity, 2022, factor_key, factor_value);
        ASSERT_EQ(expected.at(i), policy_delta);
    }
}