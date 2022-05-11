#include "pch.h"
#include "data_config.h"
#include "HealthGPS/person.h"
#include "HealthGPS/lms_model.h"
#include "HealthGPS/weight_model.h"
#include "HealthGPS/converter.h"

#include "HealthGPS.Datastore/api.h"

#include <array>

namespace fs = std::filesystem;

static auto store_full_path = default_datastore_path();

hgps::LmsModel create_lms_model(hgps::data::DataManager& manager) {
	using namespace hgps;

	auto uk = manager.get_country("GB");
	auto defintion = manager.get_disease_analysis(uk.value());
	return LmsModel{ detail::StoreConverter::to_lms_definition(defintion.lms_parameters) };
}

hgps::Person create_test_entity(
	hgps::core::Gender gender = hgps::core::Gender::male,
	int age = 20, double bmi = 23.5)
{
	auto entity = hgps::Person{};
	entity.gender = gender;
	entity.age = age;
	entity.risk_factors.emplace("bmi", bmi);

	return entity;
}

// The fixture for testing class Foo.
class WeightModelTest : public ::testing::Test {
protected:
	WeightModelTest()
		: manager{ store_full_path } {}

	hgps::data::DataManager manager;
};

TEST_F(WeightModelTest, CreateWeightModel)
{
	using namespace hgps;
	auto entity = create_test_entity();
	auto classifier = WeightModel{ create_lms_model(manager) };
	auto category = classifier.classify_weight(entity);
	ASSERT_EQ(18, classifier.child_cutoff_age());
	ASSERT_EQ(WeightCategory::normal, category);
}

TEST_F(WeightModelTest, ClassifyAdultWeight)
{
	using namespace hgps;
	using namespace hgps::core;
	auto adult_age = 20;
	auto genders = std::array<Gender,2>{ Gender::male, Gender::female };
	auto bmi_values = std::array<double, 8>{15.0, 18.4, 18.5, 24.9, 25.0, 29.9, 30.0, 40.0};
	auto expected = std::array<WeightCategory, 8>{
		WeightCategory::normal, WeightCategory::normal,
		WeightCategory::normal, WeightCategory::normal,
		WeightCategory::overweight, WeightCategory::overweight,
		WeightCategory::obese, WeightCategory::obese};

	auto classifier = WeightModel{ create_lms_model(manager) };
	for (auto& item : genders) {
		for (auto idx = 0;  auto & bmi : bmi_values) {
			auto entity = create_test_entity(item, adult_age, bmi);
			auto category = classifier.classify_weight(entity);
			ASSERT_EQ(expected[idx], category);
			idx++;
		}
	}
}

TEST_F(WeightModelTest, ClassifyChildWeight)
{
	using namespace hgps;
	using namespace hgps::core;
	auto child_age = 5;
	auto genders = std::array<Gender, 2>{ Gender::male, Gender::female };
	auto bmi_values = std::array<double, 8>{12.0, 13.0, 14.0, 15.0, 17.0, 18.0, 25.0, 30.0};
	auto expected = std::array<WeightCategory, 8>{
		WeightCategory::normal, WeightCategory::normal,
		WeightCategory::normal, WeightCategory::normal,
		WeightCategory::overweight, WeightCategory::overweight,
		WeightCategory::obese, WeightCategory::obese};

	auto classifier = WeightModel{ create_lms_model(manager) };
	for (auto& item : genders) {
		for (auto idx = 0; auto & bmi : bmi_values) {
			auto entity = create_test_entity(item, child_age, bmi);
			auto category = classifier.classify_weight(entity);
			ASSERT_EQ(expected[idx], category);
			idx++;
		}
	}
}