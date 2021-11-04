#include "pch.h"

#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/intervention_scenario.h"

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

    auto channel = SyncChannel{};
    auto scenario = BaselineScenario{ channel };

    auto factor_values = std::vector<double>{ 13.66, 7.13, 3.14, 105.0, 365.5 };
    for (auto& value : factor_values) {
        auto impact_value = scenario.apply(2010, "BMI", value);
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
    ASSERT_FALSE(period.contains(2030));
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
    ASSERT_THROW(auto period = PolicyInterval(-2022, 2021), std::out_of_range);
}

TEST(ScenarioTest, PolicyPeriodInvertedThrows)
{
    using namespace hgps;
    ASSERT_THROW(auto period = PolicyInterval(2022, 2021), std::out_of_range);
}

TEST(ScenarioTest, PolicyPeriodInvertedPeriodThrows)
{
    using namespace hgps;
    ASSERT_THROW(auto period = PolicyInterval(2023, 2022), std::out_of_range);
}

TEST(ScenarioTest, InterventionConstruction)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{"bmi", 0.02 } };
    auto period = PolicyInterval(2022);
    auto definition = PolicyDefinition{ impact_type, risk_factor, period };
    auto scenario = InterventionScenario{ channel, std::move(definition) };

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
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{"bmi", 0.02 } };
    auto period = PolicyInterval(2021, 2030);
    auto scenario = InterventionScenario{ channel,
        PolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    auto impact = scenario.apply(2023, "bmi", value);
    auto no_impact = scenario.apply(2023, "xyz", value);
    ASSERT_GT(impact, value);
    ASSERT_EQ(100.02, impact);
    ASSERT_EQ(value, no_impact);
}

TEST(ScenarioTest, InterventionApplyRelative)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto impact_type = PolicyImpactType::relative;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{"bmi", 0.02 } };
    auto period = PolicyInterval(2021, 2030);
    auto scenario = InterventionScenario{ channel,
    PolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    auto impact = scenario.apply(2025, "bmi", value);
    auto no_impact = scenario.apply(2025, "xyz", value);
    ASSERT_GT(impact, value);
    ASSERT_EQ(102.0, impact);
    ASSERT_EQ(value, no_impact);
}

TEST(ScenarioTest, InterventionApplyOutsidePeriod)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{"bmi", 0.02 } };
    auto period = PolicyInterval(2021, 2030);
    auto scenario = InterventionScenario{ channel,
    PolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    ASSERT_EQ(value, scenario.apply(2025, "xyz", value));
    ASSERT_EQ(value, scenario.apply(2010, "bmi", value));
    ASSERT_EQ(value, scenario.apply(2020, "bmi", value));
    ASSERT_EQ(value, scenario.apply(2031, "bmi", value));
    ASSERT_EQ(value, scenario.apply(2050, "bmi", value));
}

TEST(ScenarioTest, InterventionApplyOpenPeriod)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{ PolicyImpact{"bmi", 0.02 } };
    auto period = PolicyInterval(2021);
    auto scenario = InterventionScenario{ channel,
    PolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    auto expected = 100.02;
    ASSERT_EQ(value, scenario.apply(2020, "bmi", value));
    ASSERT_EQ(expected, scenario.apply(2021, "bmi", value));
    ASSERT_EQ(value, scenario.apply(2025, "bmi", value));
}

TEST(ScenarioTest, InterventionApplyMultiple)
{
    using namespace hgps;

    auto channel = SyncChannel{};
    auto impact_type = PolicyImpactType::absolute;
    auto risk_factor = std::vector<PolicyImpact>{
        PolicyImpact{"bmi", 0.02 },
        PolicyImpact{"alcohol", 0.015}
    };

    auto period = PolicyInterval(2021, 2030);
    auto scenario = InterventionScenario{ channel,
        PolicyDefinition{ impact_type, risk_factor, period } };

    auto value = 100.0;
    auto bmi_impact = scenario.apply(2023, "bmi", value);
    auto beer_impact = scenario.apply(2023, "alcohol", value);
    auto no_impact = scenario.apply(2023, "xyz", value);

    ASSERT_GT(bmi_impact, value);
    ASSERT_GT(beer_impact, value);
    ASSERT_GT(bmi_impact, beer_impact);
    ASSERT_EQ(100.02, bmi_impact);
    ASSERT_EQ(100.015, beer_impact);
    ASSERT_EQ(value, no_impact);
}