#include "pch.h"

#include "TestConsoleCapture.h"
#include "data_config.h"

#include "HealthGPS.Input/api.h"
#include "HealthGPS/api.h"
#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/event_bus.h"

#include <gtest/gtest.h>

namespace {

void register_sample_demographics(hgps::CachedRepository &repository) {
    using namespace hgps;

    std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>> region_data;
    for (const auto gender : {core::Gender::male, core::Gender::female}) {
        region_data["age_0"_id][gender]["region1"] = 0.5;
        region_data["age_0"_id][gender]["region2"] = 0.5;
    }
    repository.register_region_prevalence(region_data);

    std::map<core::Identifier,
             std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>>
        ethnicity_data;
    for (const auto age_group : {core::Identifier("Under18"), core::Identifier("Over18")}) {
        for (const auto gender : {core::Gender::male, core::Gender::female}) {
            for (const auto *region : {"region1", "region2"}) {
                ethnicity_data[age_group][gender][region]["1"] = 0.5;
                ethnicity_data[age_group][gender][region]["2"] = 0.5;
            }
        }
    }
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
    auto ses = SESDefinition{.fuction_name = "normal", .parameters = std::vector<double>{0.0, 1.0}};
    auto mapping = HierarchicalMapping(std::vector<MappingEntry>{});

    auto project_requirements = hgps::input::ProjectRequirements{};
    project_requirements.demographics.region = true;
    project_requirements.demographics.ethnicity = true;

    const std::vector<core::DiseaseInfo> diseases;
    return std::make_shared<hgps::ModelInput>(data, settings, info, ses, mapping, diseases,
                                              project_requirements, hgps::input::PIFInfo{});
}

std::unique_ptr<hgps::DemographicModule>
build_test_demographic_module(hgps::CachedRepository &repository,
                            const hgps::ModelInput &inputs) {
    register_sample_demographics(repository);
    return build_population_module(repository, inputs);
}

} // namespace

TEST(DemographicSummary, ModuleLoadSummaryBox) {
    using namespace hgps;
    using namespace hgps::input;
    using hgps::test::capture_stdout;

    core::DataTable data;
    auto inputs = create_demographic_test_input(data);

    auto manager = DataManager(test_datastore_path);
    auto repository = CachedRepository(manager);

    const auto output = capture_stdout([&] {
        auto module = build_test_demographic_module(repository, *inputs);
        ASSERT_NE(module, nullptr);
        EXPECT_GT(module->get_total_population_size(inputs->start_time()), 0u);
    });

    EXPECT_NE(output.find("Demographic module data"), std::string::npos);
    EXPECT_NE(output.find("Region assignment"), std::string::npos);
    EXPECT_NE(output.find("region1"), std::string::npos);
    EXPECT_NE(output.find("Ethnicity assignment"), std::string::npos);
}

TEST(DemographicSummary, PopulationInitAssignsRegionAndEthnicity) {
    using namespace hgps;
    using namespace hgps::input;
    using hgps::test::capture_stdout;

    core::DataTable data;
    auto inputs = create_demographic_test_input(data);

    auto manager = DataManager(test_datastore_path);
    auto repository = CachedRepository(manager);
    auto pop_module = build_test_demographic_module(repository, *inputs);

    auto bus = std::make_shared<DefaultEventBus>();
    auto channel = SyncChannel{};
    auto scenario = std::make_unique<BaselineScenario>(channel);
    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    context.reset_population(4);

    const auto output = capture_stdout([&] { pop_module->initialise_population(context); });

    EXPECT_NE(output.find("Demographic population init"), std::string::npos);
    for (const auto &person : context.population()) {
        EXPECT_FALSE(person.region.empty());
        EXPECT_FALSE(person.ethnicity.empty());
    }
}
