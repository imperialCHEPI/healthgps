#include "pch.h"

#include "HealthGPS/mapping.h"

static std::vector<hgps::MappingEntry> create_mapping_entries() {
	using namespace hgps;
	return std::vector<MappingEntry>{
		MappingEntry("Year", 0, core::Identifier::empty(), true),
		MappingEntry("Gender", 0, core::Identifier{ "gender" }),
		MappingEntry("Age", 0, core::Identifier{ "age" }),
		MappingEntry("SmokingStatus", 1),
		MappingEntry("AlcoholConsumption", 1),
		MappingEntry("BMI", 2)};
}

TEST(TestHealthGPS_Mapping, CreateEmpty)
{
	using namespace hgps;

	auto mapping = HierarchicalMapping(std::vector<MappingEntry>());

	ASSERT_EQ(0, mapping.size());
	ASSERT_EQ(0, mapping.max_level());
	ASSERT_NO_THROW(mapping.at_level(1));
}

TEST(TestHealthGPS_Mapping, CreateFull)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	auto exp_size = entries.size();
	auto mapping = HierarchicalMapping(std::move(entries));

	auto level_zero = mapping.at_level(0);
	auto level_one = mapping.at_level(1);
	auto level_two = mapping.at_level(2);

	ASSERT_EQ(exp_size, mapping.size());
	ASSERT_EQ(2, mapping.max_level());

	ASSERT_EQ(3, level_zero.size());
	ASSERT_EQ(2, level_one.size());
	ASSERT_EQ(1, level_two.size());
}

TEST(TestHealthGPS_Mapping, AccessByInterator)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	auto exp_size = entries.size();
	auto mapping = HierarchicalMapping(std::move(entries));

	ASSERT_EQ(exp_size, mapping.size());
	for (auto& entry : mapping)	{
		ASSERT_GE(entry.level(), 0);
		if (!entry.is_entity()) {
			EXPECT_EQ(entry.key(), entry.entity_key());
		}
	}
}

TEST(TestHealthGPS_Mapping, AccessByConstInterator)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	auto exp_size = entries.size();
	auto mapping = HierarchicalMapping(std::move(entries));

	ASSERT_EQ(exp_size, mapping.size());
	for (const auto& entry : mapping) {
		ASSERT_GE(entry.level(), 0);
		if (!entry.is_entity()) {
			EXPECT_EQ(entry.key(), entry.entity_key());
		}
	}
}

TEST(TestHealthGPS_Mapping, CanIdentifyDynamicFactor)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	auto mapping = HierarchicalMapping(std::move(entries));
	auto dynamic_factor = core::Identifier{ "year" };

	for (auto& entry : mapping) {
		if (entry.key() == dynamic_factor) {
			ASSERT_TRUE(entry.is_dynamic_factor());
		}
		else {
			ASSERT_FALSE(entry.is_dynamic_factor());
		}
	}
}

TEST(TestHealthGPS_Mapping, AccessAllEntries)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	auto exp_size = entries.size();
	auto mapping = HierarchicalMapping(std::move(entries));
	auto& all_entries = mapping.entries();

	ASSERT_EQ(exp_size, all_entries.size());
}

TEST(TestHealthGPS_Mapping, AccessSingleEntry)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	auto mapping = HierarchicalMapping(std::move(entries));

	auto age_key = core::Identifier{ "Age" };
	auto gender_key = core::Identifier{ "GENDER" };

	auto age_entry = mapping.at(age_key);
	auto gender_entry = mapping.at(gender_key);

	ASSERT_EQ(age_key, age_entry.key());
	ASSERT_EQ(gender_key, gender_entry.key());
}

TEST(TestHealthGPS_Mapping, AccessSingleEntryThrowForUnknowKey)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	auto test_keys = std::vector<core::Identifier>{
		core::Identifier{ "Cat" },
		core::Identifier{ "Dog" },
		core::Identifier{ "Cow" }
	};

	auto mapping = HierarchicalMapping(std::move(entries));
	for (const auto& key : test_keys)
	{
		ASSERT_THROW(mapping.at(key), std::out_of_range);
	}
}

TEST(TestHealthGPS_Mapping, AccessEntriesWithoutDynamicFactor)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	auto exp_size = entries.size() - 1u;
	auto mapping = HierarchicalMapping(std::move(entries));
	auto all_entries = mapping.entries_without_dynamic();

	ASSERT_EQ(exp_size, all_entries.size());
}

TEST(TestHealthGPS_Mapping, AccessAllLevelEntries)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	std::vector<int> exp_size = { 3, 2, 1 };
	auto mapping = HierarchicalMapping(std::move(entries));
	auto level_0_entries = mapping.at_level(0);
	auto level_1_entries = mapping.at_level(1);
	auto level_2_entries = mapping.at_level(2);

	ASSERT_EQ(exp_size[0], level_0_entries.size());
	ASSERT_EQ(exp_size[1], level_1_entries.size());
	ASSERT_EQ(exp_size[2], level_2_entries.size());
}

TEST(TestHealthGPS_Mapping, AccessLevelEntriesWithoutDynamicFactor)
{
	using namespace hgps;

	auto entries = create_mapping_entries();
	std::vector<int> exp_size = { 2, 2, 1 };
	auto mapping = HierarchicalMapping(std::move(entries));
	auto level_0_entries = mapping.at_level_without_dynamic(0);
	auto level_1_entries = mapping.at_level_without_dynamic(1);
	auto level_2_entries = mapping.at_level_without_dynamic(2);

	ASSERT_EQ(exp_size[0], level_0_entries.size());
	ASSERT_EQ(exp_size[1], level_1_entries.size());
	ASSERT_EQ(exp_size[2], level_2_entries.size());
}