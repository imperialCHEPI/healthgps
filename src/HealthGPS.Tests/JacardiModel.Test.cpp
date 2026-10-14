#include "pch.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Input/configuration.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/api.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/jacardi_education_lifecycle.h"
#include "HealthGPS/jacardi_model.h"
#include "HealthGPS/jacardi_model_update.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <string_view>

using namespace hgps;
using json = nlohmann::json;

namespace {

class JacardiTempDir {
  public:
    JacardiTempDir() {
        path_ = std::filesystem::path{::testing::TempDir()} / "jacardi" /
                std::to_string(std::random_device{}());
        std::filesystem::create_directories(path_);
    }
    ~JacardiTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

json file_slot(const std::string &name, const json &columns) {
    return json{{"name", name},
                {"format", "csv"},
                {"delimiter", ","},
                {"encoding", "ASCII"},
                {"columns", columns}};
}

void write_text(const std::filesystem::path &path, std::string_view text) {
    std::ofstream ofs{path};
    ofs << text;
}

void write_jacardi_education_csvs(const std::filesystem::path &dir) {
    // MAHIMA: Minimal strata covering init ages + Part B years used in tests.
    write_text(dir / "education_lookup.csv", "age,gender,education_id,probability\n"
                                             "22,0,3,1.0\n"
                                             "22,1,3,1.0\n"
                                             "40,0,4,1.0\n"
                                             "40,1,4,1.0\n"
                                             "110,0,5,1.0\n"
                                             "110,1,5,1.0\n");
    write_text(dir / "education_draw_at_22.csv", "year,gender,education_id,probability\n"
                                                 "2026,0,3,1.0\n"
                                                 "2026,1,3,1.0\n"
                                                 "2110,0,3,1.0\n"
                                                 "2110,1,3,1.0\n");
    write_text(dir / "education_upgrades.csv", "year,age,gender,from_id,to_id,probability\n"
                                               "2026,25,0,3,4,1.0\n"
                                               "2026,25,1,3,4,1.0\n");
}

// MAHIMA: Build slot JSON with locals so nested braces stay MSVC-friendly.
json make_static_model_json() {
    const auto lookup = file_slot("education_lookup.csv", json{{"age", "integer"},
                                                               {"gender", "integer"},
                                                               {"education_id", "integer"},
                                                               {"probability", "double"}});
    return json{{"ModelName", "JacardiModel"},
                {"Education", {{"method", "empirical_lifecycle_init"}, {"lookup", lookup}}}};
}

json make_update_model_json() {
    const auto draw = file_slot("education_draw_at_22.csv", json{{"year", "integer"},
                                                                 {"gender", "integer"},
                                                                 {"education_id", "integer"},
                                                                 {"probability", "double"}});
    const auto upgrades = file_slot("education_upgrades.csv", json{{"year", "integer"},
                                                                   {"age", "integer"},
                                                                   {"gender", "integer"},
                                                                   {"from_id", "integer"},
                                                                   {"to_id", "integer"},
                                                                   {"probability", "double"}});
    return json{{"ModelName", "JacardiModelUpdate"},
                {"Education",
                 {{"method", "empirical_lifecycle_update"},
                  {"draw_at_22", draw},
                  {"upgrade_transitions", upgrades}}}};
}

hgps::input::Configuration make_config(const std::filesystem::path &root) {
    hgps::input::Configuration config;
    config.root_path = root;
    return config;
}

std::shared_ptr<ModelInput> make_inputs(core::DataTable &data) {
    const auto country =
        core::Country{.code = 705, .name = "Slovenia", .alpha2 = "SI", .alpha3 = "SVN"};
    const auto settings = Settings{country, 0.001f, core::IntegerInterval{0, 110}};
    const auto run = RunInfo{.start_time = 2025, .stop_time = 2030, .seed = 1u};
    const auto ses = SESDefinition{.enabled = false, .fuction_name = "", .parameters = {}};
    auto mapping = HierarchicalMapping{std::vector<MappingEntry>{
        MappingEntry{"Education", 1},
    }};
    return std::make_shared<ModelInput>(data, settings, run, ses, std::move(mapping),
                                        std::vector<core::DiseaseInfo>{},
                                        hgps::input::ProjectRequirements{}, hgps::input::PIFInfo{});
}

} // namespace

TEST(JacardiModelParser, LoadsStaticDefinitionFromCsvSlots) {
    JacardiTempDir dir;
    write_jacardi_education_csvs(dir.path());

    auto definition = hgps::input::load_jacardi_risk_model_definition(make_static_model_json(),
                                                                      make_config(dir.path()));
    ASSERT_NE(definition, nullptr);

    auto model = definition->create_model();
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(RiskFactorModelType::Static, model->type());
    EXPECT_EQ("JacardiModel", model->name());
}

TEST(JacardiModelParser, LoadsUpdateDefinitionFromCsvSlots) {
    JacardiTempDir dir;
    write_jacardi_education_csvs(dir.path());

    auto definition = hgps::input::load_jacardi_update_risk_model_definition(
        make_update_model_json(), make_config(dir.path()));
    ASSERT_NE(definition, nullptr);

    auto model = definition->create_model();
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(RiskFactorModelType::Dynamic, model->type());
    EXPECT_EQ("JacardiModelUpdate", model->name());
}

TEST(JacardiModelParser, StaticMissingEducationThrows) {
    EXPECT_THROW(hgps::input::load_jacardi_risk_model_definition(
                     json{{"ModelName", "JacardiModel"}}, make_config(".")),
                 core::HgpsException);
}

TEST(JacardiModelParser, UpdateMissingEducationThrows) {
    EXPECT_THROW(hgps::input::load_jacardi_update_risk_model_definition(
                     json{{"ModelName", "JacardiModelUpdate"}}, make_config(".")),
                 core::HgpsException);
}

TEST(JacardiModelRuntime, GenerateAssignsEducationByAge) {
    JacardiTempDir dir;
    write_jacardi_education_csvs(dir.path());

    auto definition = hgps::input::load_jacardi_risk_model_definition(make_static_model_json(),
                                                                      make_config(dir.path()));
    auto model = definition->create_model();

    core::DataTable data;
    auto inputs = make_inputs(data);
    auto bus = std::make_shared<DefaultEventBus>();
    SyncChannel channel;
    auto scenario = std::make_unique<BaselineScenario>(channel);
    RuntimeContext context{bus, inputs, std::move(scenario)};
    context.reset_population(4);
    context.random().seed(1);

    context.population()[0].gender = core::Gender::male;
    context.population()[0].age = 3; // missing
    context.population()[1].gender = core::Gender::female;
    context.population()[1].age = 12; // 2
    context.population()[2].gender = core::Gender::male;
    context.population()[2].age = 20; // 3
    context.population()[3].gender = core::Gender::female;
    context.population()[3].age = 40; // lookup -> 4

    model->generate_risk_factors(context);

    EXPECT_TRUE(std::isnan(get_person_education(context.population()[0])));
    EXPECT_DOUBLE_EQ(2.0, get_person_education(context.population()[1]));
    EXPECT_DOUBLE_EQ(3.0, get_person_education(context.population()[2]));
    EXPECT_DOUBLE_EQ(4.0, get_person_education(context.population()[3]));
}

TEST(JacardiModelRuntime, StaticUpdateFillsMissingEducationOnly) {
    JacardiTempDir dir;
    write_jacardi_education_csvs(dir.path());

    auto definition = hgps::input::load_jacardi_risk_model_definition(make_static_model_json(),
                                                                      make_config(dir.path()));
    auto model = definition->create_model();

    core::DataTable data;
    auto inputs = make_inputs(data);
    auto bus = std::make_shared<DefaultEventBus>();
    SyncChannel channel;
    auto scenario = std::make_unique<BaselineScenario>(channel);
    RuntimeContext context{bus, inputs, std::move(scenario)};
    context.reset_population(2);
    context.random().seed(2);

    context.population()[0].gender = core::Gender::male;
    context.population()[0].age = 12;
    set_person_education(context.population()[0], 5.0); // already set

    context.population()[1].gender = core::Gender::female;
    context.population()[1].age = 12; // newborn-style missing Education

    model->update_risk_factors(context);

    EXPECT_DOUBLE_EQ(5.0, get_person_education(context.population()[0]));
    EXPECT_DOUBLE_EQ(2.0, get_person_education(context.population()[1]));
}

TEST(JacardiModelRuntime, DynamicUpdateAppliesPartB) {
    JacardiTempDir dir;
    write_jacardi_education_csvs(dir.path());

    auto static_def = hgps::input::load_jacardi_risk_model_definition(make_static_model_json(),
                                                                      make_config(dir.path()));
    auto update_def = hgps::input::load_jacardi_update_risk_model_definition(
        make_update_model_json(), make_config(dir.path()));
    auto static_model = static_def->create_model();
    auto update_model = update_def->create_model();

    core::DataTable data;
    auto inputs = make_inputs(data);
    auto bus = std::make_shared<DefaultEventBus>();
    SyncChannel channel;
    auto scenario = std::make_unique<BaselineScenario>(channel);
    RuntimeContext context{bus, inputs, std::move(scenario)};
    context.reset_population(3);
    context.random().seed(3);
    context.set_current_time(2026);

    // Age 6 -> set 2; age 22 draw -> 3; age 25 upgrade 3->4
    context.population()[0].gender = core::Gender::male;
    context.population()[0].age = 6;
    context.population()[1].gender = core::Gender::female;
    context.population()[1].age = 22;
    set_person_education(context.population()[1], 3.0);
    context.population()[2].gender = core::Gender::male;
    context.population()[2].age = 25;
    set_person_education(context.population()[2], 3.0);

    update_model->generate_risk_factors(context); // no-op
    update_model->update_risk_factors(context);

    EXPECT_DOUBLE_EQ(2.0, get_person_education(context.population()[0]));
    EXPECT_DOUBLE_EQ(3.0, get_person_education(context.population()[1]));
    EXPECT_DOUBLE_EQ(4.0, get_person_education(context.population()[2]));
    EXPECT_EQ("JacardiModel", static_model->name());
}
