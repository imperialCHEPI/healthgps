#include "pch.h"

#include "HealthGPS.Input/datamanager.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>

using json = nlohmann::json;

namespace {

// MAHIMA: Minimal valid data_index.json so DataManager can open a temp store.
void write_minimal_index(const std::filesystem::path &root) {
    json meta = {{"format", "csv"},
                 {"delimiter", ","},
                 {"encoding", "UTF8"},
                 {"description", "test"},
                 {"source", "test"},
                 {"url", "https://example.com"},
                 {"license", "N/A"}};

    json index;
    index["$schema"] =
        "https://raw.githubusercontent.com/imperialCHEPI/healthgps/main/schemas/v1/data_index.json";

    index["country"] = meta;
    index["country"]["path"] = "";
    index["country"]["file_name"] = "countries.csv";

    index["demographic"] = meta;
    index["demographic"]["path"] = "undb";
    index["demographic"]["age_limits"] = {0, 110};
    index["demographic"]["time_limits"] = {1950, 2100};
    index["demographic"]["projections"] = 2022;
    index["demographic"]["population"] = {{"description", "test"},
                                          {"path", "population"},
                                          {"file_name", "P{COUNTRY_CODE}.csv"}};
    index["demographic"]["mortality"] = {{"description", "test"},
                                         {"path", "mortality"},
                                         {"file_name", "M{COUNTRY_CODE}.csv"}};
    index["demographic"]["indicators"] = {{"description", "test"},
                                          {"path", "indicators"},
                                          {"file_name", "Pi{COUNTRY_CODE}.csv"}};

    index["diseases"] = meta;
    index["diseases"]["path"] = "diseases";
    index["diseases"]["age_limits"] = {0, 110};
    index["diseases"]["time_year"] = 2019;
    index["diseases"]["registry"] = json::array();
    index["diseases"]["disease"] = {
        {"path", "{CODE}"},
        {"file_name", "{CODE}.csv"},
        {"relative_risk",
         {{"path", "relative_risk"},
          {"to_disease",
           {{"path", "to_disease"}, {"file_name", "{CODE}-{TO}.csv"}, {"default_value", 1.0}}},
          {"to_risk_factor", {{"path", "to_risk_factor"}, {"file_name", "{CODE}-{TO}.csv"}}}}},
        {"parameters",
         {{"path", "params"},
          {"files",
           {{"distribution", "distribution.csv"},
            {"survival_rate", "survival_rate.csv"},
            {"death_weight", "death_weight.csv"}}}}}};

    index["analysis"] = meta;
    index["analysis"]["path"] = "analysis";
    index["analysis"]["age_limits"] = {0, 110};
    index["analysis"]["time_year"] = 2019;
    index["analysis"]["disability_file_name"] = "disability_weights.csv";
    index["analysis"]["lms_file_name"] = "lms_parameters.csv";
    index["analysis"]["cost_of_disease"] = {{"path", "cost"},
                                            {"file_name", "BoD{COUNTRY_CODE}.csv"}};

    std::ofstream{(root / "index.json")} << index.dump(2);
}

void write_text(const std::filesystem::path &path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream{path} << text;
}

class TempDataRoot {
  public:
    TempDataRoot() {
        path_ = std::filesystem::path{::testing::TempDir()} / "bod_schema" /
                std::to_string(std::random_device{}());
        std::filesystem::create_directories(path_);
        write_minimal_index(path_);
        write_text(path_ / "countries.csv", "Code,Name,Alpha2,Alpha3\n705,Slovenia,SI,SVN\n");
        write_text(path_ / "analysis" / "disability_weights.csv", "disease,weight\nother,0.1\n");
        write_text(path_ / "undb" / "indicators" / "Pi705.csv",
                   "Time,LEx,LExMale,LExFemale\n2025,80,78,82\n");
    }
    ~TempDataRoot() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST(CostOfDiseaseSchema, JacardiColumnOrderLoadsMeanByName) {
    // MAHIMA: Regression for Slovenia — Jacardi BoD CSVs put "disease" at index 12, mean at 16.
    TempDataRoot root;
    write_text(
        root.path() / "analysis" / "cost" / "BoD705.csv",
        "population_group_id,population_group_name,measure_id,measure,location_id,location,"
        "gender_id,gender,age_group_id,age_group,age,cause_id,disease,metric_id,metric_name,year,"
        "mean,upper,lower,time,is_filled\n"
        "1,All Population,3,YLD,705,Slovenia,1,Male,28,<1 year,40,294,All causes,3,Rate,2023,"
        "0.25,0.3,0.2,2023,True\n"
        "1,All Population,3,YLD,705,Slovenia,2,Female,28,<1 year,40,294,All causes,3,Rate,2023,"
        "0.35,0.4,0.3,2023,True\n");

    hgps::input::DataManager manager{root.path()};
    const auto country = manager.get_country("SVN");
    const auto entity = manager.get_disease_analysis(country);

    ASSERT_TRUE(entity.cost_of_diseases.contains(40));
    EXPECT_DOUBLE_EQ(0.25, entity.cost_of_diseases.at(40).at(hgps::core::Gender::male));
    EXPECT_DOUBLE_EQ(0.35, entity.cost_of_diseases.at(40).at(hgps::core::Gender::female));
}

TEST(CostOfDiseaseSchema, LegacyColumnOrderStillLoads) {
    TempDataRoot root;
    write_text(root.path() / "analysis" / "cost" / "BoD705.csv",
               "location_id,location,disease,time,age_group_id,age_group,age,is_filled,gender_id,"
               "gender,measure_id,measure,mean,lower,upper\n"
               "705,Slovenia,All causes,2021,5,1 to 4,40,TRUE,1,Male,3,YLD,0.15,0.1,0.2\n"
               "705,Slovenia,All causes,2021,5,1 to 4,40,TRUE,2,Female,3,YLD,0.18,0.1,0.2\n");

    hgps::input::DataManager manager{root.path()};
    const auto country = manager.get_country("SVN");
    const auto entity = manager.get_disease_analysis(country);

    ASSERT_TRUE(entity.cost_of_diseases.contains(40));
    EXPECT_DOUBLE_EQ(0.15, entity.cost_of_diseases.at(40).at(hgps::core::Gender::male));
    EXPECT_DOUBLE_EQ(0.18, entity.cost_of_diseases.at(40).at(hgps::core::Gender::female));
}
