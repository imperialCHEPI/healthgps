#include "HealthGPS.Core/disease.h"
#include "HealthGPS.Input/pif_data.h"
#include "pch.h"

using namespace hgps::input;
using namespace hgps::core;

TEST(PIFDataItem, DefaultConstruction) {
    PIFDataItem item;
    EXPECT_EQ(0, item.age);
    EXPECT_EQ(Gender::female, item.gender);
    EXPECT_EQ(0, item.year_post_intervention);
    EXPECT_DOUBLE_EQ(0.0, item.pif_value);
}

TEST(PIFDataItem, ValueConstruction) {
    PIFDataItem item{25, Gender::male, 5, 0.3};
    EXPECT_EQ(25, item.age);
    EXPECT_EQ(Gender::male, item.gender);
    EXPECT_EQ(5, item.year_post_intervention);
    EXPECT_NEAR(0.3, item.pif_value, 1e-6);
}

TEST(PIFTable, EmptyTable) {
    PIFTable table;
    EXPECT_FALSE(table.has_data());
    EXPECT_EQ(0, table.size());
    EXPECT_DOUBLE_EQ(0.0, table.get_pif_value(25, Gender::male, 5));
}

TEST(PIFTable, AddItems) {
    PIFTable table;
    PIFDataItem item1{25, Gender::male, 5, 0.3};
    PIFDataItem item2{30, Gender::female, 3, 0.2};
    PIFDataItem item3{25, Gender::male, 10, 0.4};

    table.add_item(item1);
    table.add_item(item2);
    table.add_item(item3);

    EXPECT_TRUE(table.has_data());
    EXPECT_EQ(3, table.size());
}

TEST(PIFTable, GetPIFValues) {
    PIFTable table;
    PIFDataItem item1{25, Gender::male, 5, 0.3};
    PIFDataItem item2{30, Gender::female, 3, 0.2};
    PIFDataItem item3{25, Gender::male, 10, 0.4};

    table.add_item(item1);
    table.add_item(item2);
    table.add_item(item3);

    // Exact match
    EXPECT_NEAR(0.3, table.get_pif_value(25, Gender::male, 5), 1e-6);
    EXPECT_NEAR(0.2, table.get_pif_value(30, Gender::female, 3), 1e-6);
    EXPECT_NEAR(0.4, table.get_pif_value(25, Gender::male, 10), 1e-6);

    // No match - should return 0.0
    EXPECT_DOUBLE_EQ(0.0, table.get_pif_value(25, Gender::female, 5));
    EXPECT_DOUBLE_EQ(0.0, table.get_pif_value(30, Gender::male, 3));
    EXPECT_DOUBLE_EQ(0.0, table.get_pif_value(25, Gender::male, 15));
}

TEST(PIFData, EmptyData) {
    PIFData pif_data;
    EXPECT_FALSE(pif_data.has_data());
    EXPECT_EQ(nullptr, pif_data.get_scenario_data("Scenario1"));
}

TEST(PIFData, AddScenarioData) {
    PIFData pif_data;
    PIFTable table1;
    table1.add_item({25, Gender::male, 5, 0.3});
    table1.add_item({30, Gender::female, 3, 0.2});

    PIFTable table2;
    table2.add_item({25, Gender::male, 5, 0.4});
    table2.add_item({30, Gender::female, 3, 0.3});

    pif_data.add_scenario_data("Scenario1", std::move(table1));
    pif_data.add_scenario_data("Scenario2", std::move(table2));

    EXPECT_TRUE(pif_data.has_data());

    auto *scenario1 = pif_data.get_scenario_data("Scenario1");
    auto *scenario2 = pif_data.get_scenario_data("Scenario2");
    auto *scenario3 = pif_data.get_scenario_data("Scenario3");

    EXPECT_NE(nullptr, scenario1);
    EXPECT_NE(nullptr, scenario2);
    EXPECT_EQ(nullptr, scenario3);

    EXPECT_EQ(2, scenario1->size());
    EXPECT_EQ(2, scenario2->size());
}

TEST(PIFData, GetScenarioData) {
    PIFData pif_data;
    PIFTable table;
    table.add_item({25, Gender::male, 5, 0.3});
    pif_data.add_scenario_data("Scenario1", std::move(table));

    auto *scenario = pif_data.get_scenario_data("Scenario1");
    EXPECT_NE(nullptr, scenario);
    EXPECT_TRUE(scenario->has_data());
    EXPECT_NEAR(0.3, scenario->get_pif_value(25, Gender::male, 5), 1e-6);
}
