#include "HealthGPS.Core/country.h"
#include "HealthGPS.Core/disease.h"
#include "HealthGPS.Input/pif_data.h"
#include "pch.h"

using namespace hgps;
using namespace hgps::core;
using namespace hgps::input;

TEST(PIFIntegration, PIFDataWorkflow) {
    // Test the complete PIF data workflow
    hgps::input::PIFData data;
    hgps::input::PIFTable table;

    // Add test data
    table.add_item({25, Gender::male, 5, 0.3});
    table.add_item({30, Gender::female, 3, 0.2});
    table.add_item({35, Gender::male, 10, 0.4});

    // PHASE 1 OPTIMIZATION: Build hash table for O(1) lookups
    table.build_hash_table();

    data.add_scenario_data("Scenario1", std::move(table));

    // Verify data structure
    EXPECT_TRUE(data.has_data());

    const auto *scenario = data.get_scenario_data("Scenario1");
    EXPECT_NE(nullptr, scenario);
    EXPECT_TRUE(scenario->has_data());
    EXPECT_EQ(3, scenario->size());

    // Test data retrieval
    EXPECT_NEAR(0.3, scenario->get_pif_value(25, Gender::male, 5), 1e-6);
    EXPECT_NEAR(0.2, scenario->get_pif_value(30, Gender::female, 3), 1e-6);
    EXPECT_NEAR(0.4, scenario->get_pif_value(35, Gender::male, 10), 1e-6);

    // Test non-existent data
    EXPECT_DOUBLE_EQ(0.0, scenario->get_pif_value(25, Gender::female, 5));
    EXPECT_DOUBLE_EQ(0.0, scenario->get_pif_value(30, Gender::male, 3));
}

TEST(PIFIntegration, MultipleScenarios) {
    hgps::input::PIFData data;

    // Add Scenario1 data
    hgps::input::PIFTable table1;
    table1.add_item({25, Gender::male, 5, 0.3}); // 0 = male in CSV format

    // PHASE 1 OPTIMIZATION: Build hash table for O(1) lookups
    table1.build_hash_table();

    data.add_scenario_data("Scenario1", std::move(table1));

    // Add Scenario2 data
    hgps::input::PIFTable table2;
    table2.add_item({25, Gender::male, 5, 0.5});

    // PHASE 1 OPTIMIZATION: Build hash table for O(1) lookups
    table2.build_hash_table();

    data.add_scenario_data("Scenario2", std::move(table2));

    EXPECT_TRUE(data.has_data());

    const auto *scenario1 = data.get_scenario_data("Scenario1");
    const auto *scenario2 = data.get_scenario_data("Scenario2");
    const auto *scenario3 = data.get_scenario_data("Scenario3");

    EXPECT_NE(nullptr, scenario1);
    EXPECT_NE(nullptr, scenario2);
    EXPECT_EQ(nullptr, scenario3);

    EXPECT_NEAR(0.3, scenario1->get_pif_value(25, Gender::male, 5), 1e-6);
    EXPECT_NEAR(0.5, scenario2->get_pif_value(25, Gender::male, 5), 1e-6);
}
