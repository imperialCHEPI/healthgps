#include "HealthGPS.Input/poco.h"
#include "pch.h"

using namespace hgps::input;

TEST(RepositoryPIF, PIFConfiguration) {
    PIFInfo pif_info{true, "data", "Smoking", "Scenario1"};

    EXPECT_TRUE(pif_info.enabled);
    EXPECT_EQ("data", pif_info.data_root_path);
    EXPECT_EQ("Smoking", pif_info.risk_factor);
    EXPECT_EQ("Scenario1", pif_info.scenario);
}

TEST(RepositoryPIF, PIFInfoStructure) {
    PIFInfo info{true, "data", "Smoking", "Scenario1"};
    EXPECT_TRUE(info.enabled);
    EXPECT_EQ("data", info.data_root_path);
    EXPECT_EQ("Smoking", info.risk_factor);
    EXPECT_EQ("Scenario1", info.scenario);

    PIFInfo info2{false, "other", "Diet", "Scenario2"};
    EXPECT_FALSE(info2.enabled);
    EXPECT_EQ("other", info2.data_root_path);
    EXPECT_EQ("Diet", info2.risk_factor);
    EXPECT_EQ("Scenario2", info2.scenario);

    EXPECT_NE(info, info2);
}
