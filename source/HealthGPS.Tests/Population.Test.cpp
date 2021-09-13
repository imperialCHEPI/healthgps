#include "pch.h"

#include "HealthGPS\api.h"

TEST(TestHealthGPS_Population, CreateDefaultPerson)
{
	using namespace hgps;

	auto p = Person{};
	ASSERT_GT(p.id(), 0);
	ASSERT_EQ(0, p.age);
	ASSERT_EQ(core::Gender::unknown, p.gender);
	ASSERT_TRUE(p.is_alive);
	ASSERT_FALSE(p.has_emigrated);
	ASSERT_TRUE(p.is_active());
	ASSERT_EQ(0, p.time_of_death);
	ASSERT_EQ(0, p.education.value());
	ASSERT_EQ(0, p.education.old_value());
	ASSERT_EQ(0, p.income.value());
	ASSERT_EQ(0, p.income.old_value());
	ASSERT_EQ(0, p.risk_factors.size());
	ASSERT_EQ(0, p.diseases.size());
}

TEST(TestHealthGPS_Population, CreateUniquePerson)
{
	using namespace hgps;

	auto p1 = Person{};
	auto p2 = Person{};
	auto p3 = p1;

	ASSERT_GT(p1.id(), 0);
	ASSERT_GT(p2.id(), p1.id());
	ASSERT_EQ(p1.id(), p3.id());
}

TEST(TestHealthGPS_Population, PersonStateIsActive)
{
	using namespace hgps;

	auto p1 = Person{};
	auto p2 = Person{};
	auto p3 = Person{};
	auto p4 = p2;

	p2.is_alive = false;
	p3.has_emigrated = true;

	ASSERT_TRUE(p1.is_active());
	ASSERT_FALSE(p2.is_active());
	ASSERT_FALSE(p3.is_active());
	ASSERT_TRUE(p4.is_active());
}

TEST(TestHealthGPS_Population, CreateDefaultTwoStepValue)
{
	using namespace hgps;

	auto v = TwoStepValue<int>{};
	ASSERT_EQ(0, v());
	ASSERT_EQ(0, v.value());
	ASSERT_EQ(0, v.old_value());
}

TEST(TestHealthGPS_Population, CreateCustomTwoStepValue)
{
	using namespace hgps;

	auto v = TwoStepValue<int>{5};
	ASSERT_EQ(5, v());
	ASSERT_EQ(5, v.value());
	ASSERT_EQ(0, v.old_value());
}

TEST(TestHealthGPS_Population, AssignToTwoStepValue)
{
	using namespace hgps;

	auto v = TwoStepValue<int>{ 5 };
	v = 10;
	v.set_value(v.value() + 3);

	ASSERT_EQ(13, v());
	ASSERT_EQ(13, v.value());
	ASSERT_EQ(10, v.old_value());
}

TEST(TestHealthGPS_Population, SetBothTwoStepValues)
{
	using namespace hgps;

	auto v = TwoStepValue<int>{};
	v.set_both_values(15);

	ASSERT_EQ(15, v());
	ASSERT_EQ(15, v.value());
	ASSERT_EQ(15, v.old_value());
}

TEST(TestHealthGPS_Population, CloneTwoStepValues)
{
	using namespace hgps;

	auto source = TwoStepValue<int>{5};
	source = 10;

	auto clone = source.clone();

	ASSERT_EQ(source(), clone());
	ASSERT_EQ(source.value(), clone.value());
	ASSERT_EQ(source.old_value(), clone.old_value());
	ASSERT_NE(std::addressof(source), std::addressof(clone));
}

TEST(TestHealthGPS_Population, CreateDefaultDisease)
{
	using namespace hgps;

	auto v = Disease{};

	ASSERT_EQ(DiseaseStatus::free, v.status);
	ASSERT_EQ(0, v.start_time);
}

TEST(TestHealthGPS_Population, CloneDiseaseType)
{
	using namespace hgps;

	auto source = Disease{.status = DiseaseStatus::active, .start_time = 2021};
	auto clone = source.clone();

	ASSERT_EQ(source.status, clone.status);
	ASSERT_EQ(source.start_time, clone.start_time);
	ASSERT_NE(std::addressof(source), std::addressof(clone));
}