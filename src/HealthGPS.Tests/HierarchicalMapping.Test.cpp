#include "pch.h"

#include "HealthGPS/mapping.h"

namespace {
std::vector<hgps::MappingEntry> mapping_entries{
    {{"Gender", 0}, {"Age", 0}, {"SmokingStatus", 1}, {"AlcoholConsumption", 1}, {"BMI", 2}}};
} // anonymous namespace

TEST(TestHealthGPS_Mapping, CreateEmpty) {
    using namespace hgps;

    auto mapping = HierarchicalMapping(std::vector<MappingEntry>());

    ASSERT_EQ(0, mapping.size());
    ASSERT_EQ(0, mapping.max_level());
    ASSERT_NO_THROW(mapping.at_level(1));
}

TEST(TestHealthGPS_Mapping, CreateFull) {
    using namespace hgps;

    auto mapping = HierarchicalMapping(mapping_entries);

    auto level_zero = mapping.at_level(0);
    auto level_one = mapping.at_level(1);
    auto level_two = mapping.at_level(2);

    ASSERT_EQ(mapping_entries.size(), mapping.size());
    ASSERT_EQ(2, mapping.max_level());

    ASSERT_EQ(2, level_zero.size());
    ASSERT_EQ(2, level_one.size());
    ASSERT_EQ(1, level_two.size());
}

TEST(TestHealthGPS_Mapping, AccessByInterator) {
    using namespace hgps;

    auto mapping = HierarchicalMapping(mapping_entries);

    ASSERT_EQ(mapping_entries.size(), mapping.size());
    for (auto &entry : mapping) {
        ASSERT_GE(entry.level(), 0);
    }
}

TEST(TestHealthGPS_Mapping, AccessByConstInterator) {
    using namespace hgps;

    auto mapping = HierarchicalMapping(mapping_entries);

    ASSERT_EQ(mapping_entries.size(), mapping.size());
    for (const auto &entry : mapping) {
        ASSERT_GE(entry.level(), 0);
    }
}

TEST(TestHealthGPS_Mapping, AccessAllEntries) {
    using namespace hgps;

    auto mapping = HierarchicalMapping(mapping_entries);
    const auto &all_entries = mapping.entries();

    ASSERT_EQ(mapping_entries.size(), all_entries.size());
}

TEST(TestHealthGPS_Mapping, AccessSingleEntry) {
    using namespace hgps;

    auto mapping = HierarchicalMapping(mapping_entries);

    auto age_key = core::Identifier{"Age"};
    auto gender_key = core::Identifier{"GENDER"};

    auto age_entry = mapping.at(age_key);
    auto gender_entry = mapping.at(gender_key);

    ASSERT_EQ(age_key, age_entry.key());
    ASSERT_EQ(gender_key, gender_entry.key());
}

TEST(TestHealthGPS_Mapping, AccessSingleEntryThrowForUnknowKey) {
    using namespace hgps;

    auto test_keys = std::vector<core::Identifier>{core::Identifier{"Cat"}, core::Identifier{"Dog"},
                                                   core::Identifier{"Cow"}};

    auto mapping = HierarchicalMapping(mapping_entries);
    for (const auto &key : test_keys) {
        ASSERT_THROW(mapping.at(key), std::out_of_range);
    }
}

TEST(TestHealthGPS_Mapping, AccessAllLevelEntries) {
    using namespace hgps;

    std::vector<int> exp_size = {2, 2, 1};
    auto mapping = HierarchicalMapping(mapping_entries);
    auto level_0_entries = mapping.at_level(0);
    auto level_1_entries = mapping.at_level(1);
    auto level_2_entries = mapping.at_level(2);

    ASSERT_EQ(exp_size[0], level_0_entries.size());
    ASSERT_EQ(exp_size[1], level_1_entries.size());
    ASSERT_EQ(exp_size[2], level_2_entries.size());
}
