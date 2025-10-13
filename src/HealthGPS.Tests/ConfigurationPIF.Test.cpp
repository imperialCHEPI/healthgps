#include "HealthGPS.Input/configuration.h"
#include "HealthGPS.Input/poco.h"
#include "pch.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace hgps::input;

TEST(ConfigurationPIF, PIFInfoStructure) {
    PIFInfo info;
    EXPECT_FALSE(info.enabled);
    EXPECT_TRUE(info.data_root_path.empty());
    EXPECT_TRUE(info.risk_factor.empty());
    EXPECT_TRUE(info.scenario.empty());

    PIFInfo info2{.enabled = true,
                  .data_root_path = "data",
                  .risk_factor = "Smoking",
                  .scenario = "Scenario1"};
    EXPECT_TRUE(info2.enabled);
    EXPECT_EQ("data", info2.data_root_path);
    EXPECT_EQ("Smoking", info2.risk_factor);
    EXPECT_EQ("Scenario1", info2.scenario);

    EXPECT_NE(info, info2);
}

TEST(ConfigurationPIF, PIFInfoEquality) {
    PIFInfo info1{.enabled = true,
                  .data_root_path = "data",
                  .risk_factor = "Smoking",
                  .scenario = "Scenario1"};
    PIFInfo info2{.enabled = true,
                  .data_root_path = "data",
                  .risk_factor = "Smoking",
                  .scenario = "Scenario1"};
    PIFInfo info3{.enabled = false,
                  .data_root_path = "data",
                  .risk_factor = "Smoking",
                  .scenario = "Scenario1"};

    EXPECT_EQ(info1, info2);
    EXPECT_NE(info1, info3);
}
