#include "pch.h"

#include "HealthGPS/analysis_module.h"

#include "simulation.h"

hgps::Person create_test_person(int age = 20, hgps::core::Gender gender = hgps::core::Gender::male) {
    auto person = hgps::Person{};
    person.age = age;
    person.gender = gender;
    return person;
}

namespace hgps {
class TestAnalysisModule : public ::testing::Test {
    protected:
        hgps::core::DataTable data;
        hgps::input::DataManager manager = hgps::input::DataManager(test_datastore_path);
        hgps::CachedRepository repository = hgps::CachedRepository(manager);

        hgps::ModelInput inputs = create_test_configuration(data);

        std::unique_ptr<hgps::AnalysisModule> analysis_module = build_analysis_module(repository, inputs);

        hgps::Person test_person = create_test_person(20, hgps::core::Gender::male);

        TestAnalysisModule() {
            create_test_datatable(data);
        }
    };

TEST_F(TestAnalysisModule, CalculateIndex) {
    // Test that the index is calculated correctly
    size_t index = analysis_module->calculate_index(test_person);

    ASSERT_EQ(1, 1);
}

} // namespace hgps
