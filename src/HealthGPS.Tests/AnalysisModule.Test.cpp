#include "pch.h"

#include "HealthGPS/analysis_module.h"
#include "HealthGPS/converter.h"
#include "HealthGPS/lms_model.h"
#include "data_config.h"
#include "gender_table.h"

#include <datamanager.h>

hgps::LmsDefinition lms_parameters;

hgps::LmsModel create_lms_model(hgps::input::DataManager &manager) {
    using namespace hgps;

    auto parameters = manager.get_lms_parameters();
    lms_parameters = detail::StoreConverter::to_lms_definition(parameters);
    return LmsModel{lms_parameters};
}

hgps::Person create_test_person(int age = 20) {
    auto person = hgps::Person{};
    person.age = age;
    return person;
}

hgps::AnalysisModule create_test_analysis_module() {
    hgps::GenderTable<int, float> life_expectancy;
    hgps::DoubleAgeGenderTable observed_YLD;
    std::map<hgps::core::Identifier, float> disability_weights;
    auto definition = hgps::AnalysisDefinition{life_expectancy, observed_YLD, disability_weights};

    // Create WeightModel
    hgps::input::DataManager manager;
    auto classifier = hgps::WeightModel{create_lms_model(manager)};

    auto age_range = core::IntegerInterval{};
    auto comorbidities = 0;

    return hgps::AnalysisModule{definition, classifier, age_range, comorbidities};
}

class TestAnalysisModule : public ::testing::Test {
  protected:
    hgps::AnalysisModule analysis_module = create_test_analysis_module();

    analysis_module.factor_bins_ = std::vector<size_t>{100};
    analysis_module.factor_bin_widths = std::vector<double>{10.0};
    analysis_module.factor_min_values = std::vector<double>{1.0};
    analysis_module.channels_ =
        std::vector<std::string>{"test_channel_1", "test_channel_2", "test_channel_3"};
    analysis_module.factors_to_calculate = std::vector<core::Identifier>{"Age"_id};

    hgps::Person test_person = create_test_person(20);
};

TEST_F(TestAnalysisModule, CalculateIndex) {
    // Test that the index is calculated correctly
}
