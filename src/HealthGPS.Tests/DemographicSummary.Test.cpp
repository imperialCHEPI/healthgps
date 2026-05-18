#include "pch.h"

#include "data_config.h"

#include "HealthGPS.Input/api.h"
#include "HealthGPS/api.h"
#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/event_bus.h"

#include <functional>
#include <gtest/gtest.h>
#include <sstream>

namespace {

std::string capture_stdout(const std::function<void()> &action) {
    std::ostringstream buffer;
    auto *original = std::cout.rdbuf(buffer.rdbuf());
    action();
    std::cout.rdbuf(original);
    return buffer.str();
}

void register_sample_demographics(hgps::CachedRepository &repository) {
    using namespace hgps;

    std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>> region_data;
    region_data["age_0"_id][core::Gender::male]["region1"] = 0.6;
    region_data["age_0"_id][core::Gender::male]["region2"] = 0.4;
    repository.register_region_prevalence(region_data);

    std::map<core::Identifier,
             std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>>
        ethnicity_data;
    ethnicity_data["Under18"_id][core::Gender::male]["region1"]["1"] = 0.7;
    ethnicity_data["Under18"_id][core::Gender::male]["region1"]["2"] = 0.3;
    repository.register_ethnicity_prevalence(ethnicity_data);
}

std::shared_ptr<hgps::ModelInput> create_demographic_test_input(hgps::core::DataTable &data) {
    using namespace hgps;
    using namespace hgps::core;

    auto gender_values = std::vector<int>{1, 0};
    auto age_values = std::vector<int>{0, 25};
    auto edu_values = std::vector<float>{6.0f, 12.0f};
    auto inc_values = std::vector<double>{2.0, 10.0};

    auto gender_builder = IntegerDataTableColumnBuilder{"Gender"};
    auto age_builder = IntegerDataTableColumnBuilder{"Age"};
    auto edu_builder = FloatDataTableColumnBuilder{"Education"};
    auto inc_builder = DoubleDataTableColumnBuilder{"Income"};

    for (size_t i = 0; i < gender_values.size(); i++) {
        gender_builder.append(gender_values[i]);
        age_builder.append(age_values[i]);
        edu_builder.append(edu_values[i]);
        inc_builder.append(inc_values[i]);
    }

    data.add(gender_builder.build());
    data.add(age_builder.build());
    data.add(edu_builder.build());
    data.add(inc_builder.build());

    auto uk = Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};
    auto settings = Settings{uk, 0.1f, IntegerInterval(0, 30)};
    auto info = RunInfo{.start_time = 2018, .stop_time = 2025, .seed = std::nullopt};
    auto ses_mapping = std::map<std::string, std::string>{
        {"gender", "Gender"}, {"age", "Age"}, {"education", "Education"}, {"income", "Income"}};
    auto ses = SESDefinition{.fuction_name = "normal", .parameters = std::vector<double>{0.0, 1.0}};
    auto mapping = HierarchicalMapping(std::vector<MappingEntry>{});

    auto project_requirements = hgps::input::ProjectRequirements{};
    project_requirements.demographics.region = true;
    project_requirements.demographics.ethnicity = true;

    const std::vector<core::DiseaseInfo> diseases;
    return std::make_shared<hgps::ModelInput>(data, settings, info, ses, mapping, diseases,
                                              project_requirements, hgps::input::PIFInfo{});
}

} // namespace

TEST(DemographicSummary, ModuleLoadSummaryBox) {
    using namespace hgps;
    using namespace hgps::input;

    core::DataTable data;
    auto inputs = create_demographic_test_input(data);

    auto manager = DataManager(test_datastore_path);
    auto repository = CachedRepository(manager);
    register_sample_demographics(repository);

    const auto output = capture_stdout([&] {
        auto module = build_population_module(repository, *inputs);
        ASSERT_NE(module, nullptr);
    });

    EXPECT_NE(output.find("Demographic module data"), std::string::npos);
    EXPECT_NE(output.find("Region assignment"), std::string::npos);
    EXPECT_NE(output.find("region1"), std::string::npos);
    EXPECT_NE(output.find("Ethnicity assignment"), std::string::npos);
}

TEST(DemographicSummary, PopulationInitSummaryBox) {
    using namespace hgps;
    using namespace hgps::input;

    core::DataTable data;
    auto inputs = create_demographic_test_input(data);

    auto manager = DataManager(test_datastore_path);
    auto repository = CachedRepository(manager);
    register_sample_demographics(repository);

    auto pop_module = build_population_module(repository, *inputs);

    auto bus = std::make_shared<DefaultEventBus>();
    auto channel = SyncChannel{};
    auto scenario = std::make_unique<BaselineScenario>(channel);
    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    context.reset_population(4);

    const auto output = capture_stdout([&] { pop_module->initialise_population(context); });

    EXPECT_NE(output.find("Demographic population init"), std::string::npos);
    EXPECT_NE(output.find("Population"), std::string::npos);
    EXPECT_NE(output.find("Assignment"), std::string::npos);
}
