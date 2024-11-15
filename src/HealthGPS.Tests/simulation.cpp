#include "simulation.h"

namespace {
std::vector<hgps::MappingEntry> mapping_entries{
    {{"Gender", 0}, {"Age", 0}, {"SmokingStatus", 1}, {"AlcoholConsumption", 1}, {"BMI", 2}}};
} // anonymous namespace

void create_test_datatable(hgps::core::DataTable &data) {
    using namespace hgps;
    using namespace hgps::core;

    auto gender_values = std::vector<int>{1, 0, 0, 1, 0};
    auto age_values = std::vector<int>{4, 9, 14, 19, 25};
    auto edu_values = std::vector<float>{6.0f, 10.0f, 2.0f, 9.0f, 12.0f};
    auto inc_values = std::vector<double>{2.0, 10.0, 5.0, std::nan(""), 13.0};

    auto gender_builder = IntegerDataTableColumnBuilder{"Gender"};
    auto age_builder = IntegerDataTableColumnBuilder{"Age"};
    auto edu_builder = FloatDataTableColumnBuilder{"Education"};
    auto inc_builder = DoubleDataTableColumnBuilder{"Income"};

    for (size_t i = 0; i < gender_values.size(); i++) {
        gender_builder.append(gender_values[i]);
        age_builder.append(age_values[i]);
        edu_builder.append(edu_values[i]);
        if (std::isnan(inc_values[i])) {
            inc_builder.append_null();
        } else {
            inc_builder.append(inc_values[i]);
        }
    }

    data.add(gender_builder.build());
    data.add(age_builder.build());
    data.add(edu_builder.build());
    data.add(inc_builder.build());
}

std::shared_ptr<hgps::ModelInput> create_test_configuration(hgps::core::DataTable &data) {
    using namespace hgps;
    using namespace hgps::core;

    auto uk = core::Country{.code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR"};

    auto age_range = core::IntegerInterval(0, 30);
    auto settings = Settings{uk, 0.1f, age_range};
    auto info = RunInfo{.start_time = 2018, .stop_time = 2025, .seed = std::nullopt};
    auto ses_mapping = std::map<std::string, std::string>{
        {"gender", "Gender"}, {"age", "Age"}, {"education", "Education"}, {"income", "Income"}};
    auto ses = SESDefinition{.fuction_name = "normal", .parameters = std::vector<double>{0.0, 1.0}};

    auto mapping = HierarchicalMapping(mapping_entries);

    auto diseases = std::vector<core::DiseaseInfo>{
        DiseaseInfo{
            .group = DiseaseGroup::other, .code = core::Identifier{"asthma"}, .name = "Asthma"},
        DiseaseInfo{.group = DiseaseGroup::other,
                    .code = core::Identifier{"diabetes"},
                    .name = "Diabetes Mellitus"},
        DiseaseInfo{.group = DiseaseGroup::cancer,
                    .code = core::Identifier{"colorectalcancer"},
                    .name = "Colorectal cancer"},
    };

    return std::make_shared<hgps::ModelInput>(data, settings, info, ses, mapping, diseases);
}
