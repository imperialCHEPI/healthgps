#include "HealthGPS.Core/disease.h"
#include "HealthGPS.Core/identifier.h"
#include "HealthGPS.Input/pif_data.h"
#include "pch.h"

using namespace hgps;
using namespace hgps::core;
using namespace hgps::input;

TEST(DiseaseModelPIF, PIFDataIntegration) {
    // Test PIF data structure integration
    hgps::input::PIFData data;
    hgps::input::PIFTable table;

    table.add_item({25, Gender::male, 5, 0.3});
    table.add_item({30, Gender::female, 3, 0.2});
    data.add_scenario_data("Scenario1", std::move(table));

    EXPECT_TRUE(data.has_data());

    auto *scenario = data.get_scenario_data("Scenario1");
    EXPECT_NE(nullptr, scenario);
    EXPECT_TRUE(scenario->has_data());
    EXPECT_EQ(2, scenario->size());

    EXPECT_NEAR(0.3, scenario->get_pif_value(25, Gender::male, 5), 1e-6);
    EXPECT_NEAR(0.2, scenario->get_pif_value(30, Gender::female, 3), 1e-6);
    EXPECT_DOUBLE_EQ(0.0, scenario->get_pif_value(25, Gender::female, 5)); // No match
}

TEST(DiseaseModelPIF, PIFDataItem) {
    hgps::input::PIFDataItem item{25, Gender::male, 5, 0.3};
    EXPECT_EQ(25, item.age);
    EXPECT_EQ(Gender::male, item.gender);
    EXPECT_EQ(5, item.year_post_intervention);
    EXPECT_NEAR(0.3, item.pif_value, 1e-6);
}

TEST(DiseaseModelPIF, PIFTable) {
    hgps::input::PIFTable table;
    EXPECT_FALSE(table.has_data());
    EXPECT_EQ(0, table.size());

    table.add_item({25, Gender::male, 5, 0.3});
    EXPECT_TRUE(table.has_data());
    EXPECT_EQ(1, table.size());
    EXPECT_NEAR(0.3, table.get_pif_value(25, Gender::male, 5), 1e-6);
}
