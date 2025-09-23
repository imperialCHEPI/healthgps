#include "HealthGPS.Core/country.h"
#include "HealthGPS.Core/disease.h"
#include "HealthGPS.Input/datamanager.h"
#include "pch.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace hgps::input;
using namespace hgps::core;

TEST(DataManagerPIF, BasicDataManagerCreation) {
    auto temp_dir = std::filesystem::temp_directory_path() / "healthgps_pif_test";
    std::filesystem::create_directories(temp_dir);

    nlohmann::json index_json = {
        {"$schema", "data_index.json"},
        {"diseases",
         {{"registry", nlohmann::json::array()},
          {"path", "diseases"},
          {"disease",
           {{"path", "disease_{disease_code}"}, {"file_name", "disease_{country_code}.csv"}}}}}};

    auto index_file = temp_dir / "index.json";
    std::ofstream index_stream(index_file);
    index_stream << index_json.dump();
    index_stream.close();

    DataManager manager(temp_dir, VerboseMode::none);

    // Test that DataManager was created successfully
    EXPECT_TRUE(std::filesystem::exists(temp_dir));

    std::filesystem::remove_all(temp_dir);
}

TEST(DataManagerPIF, PIFDataStructures) {
    // Test PIF data structures directly
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
}

TEST(DataManagerPIF, PIFDataItem) {
    hgps::input::PIFDataItem item{25, Gender::male, 5, 0.3};
    EXPECT_EQ(25, item.age);
    EXPECT_EQ(Gender::male, item.gender);
    EXPECT_EQ(5, item.year_post_intervention);
    EXPECT_NEAR(0.3, item.pif_value, 1e-6);
}
