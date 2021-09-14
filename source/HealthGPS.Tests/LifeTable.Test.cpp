#include "pch.h"

#include "HealthGPS\life_table.h"

TEST(TestHealthGPS_LifeTable, CreateEmpty)
{
	using namespace hgps;

	auto table = LifeTable(std::map<int, Birth>{}, std::map<int, std::map<int, Mortality>>{});

	ASSERT_TRUE(table.empty());
}

TEST(TestHealthGPS_LifeTable, CreateFull)
{
	using namespace hgps;

	auto births = std::map<int, Birth>{
		{2020, Birth(10.0f, 0.90f)},
		{2021, Birth(15.0f, 0.85f)},
		{2022, Birth(13.5f, 0.95f)},
	};

	auto deaths = std::map<int, std::map<int, Mortality>>{};
	deaths.emplace(2020, std::map<int, Mortality>{
		{1, Mortality(5.0f, 6.0f)}, { 2, Mortality(5.5f, 6.5f) }});

	deaths.emplace(2021, std::map<int, Mortality>{
		{1, Mortality(6.0f, 6.5f)}, { 2, Mortality(7.0f, 7.3f) } });

	deaths.emplace(2022, std::map<int, Mortality>{
		{1, Mortality(7.0f, 7.3f)}, { 2, Mortality(7.5f, 8.1f) } });

	auto table = LifeTable(std::move(births), std::move(deaths));

	auto tolerance = 1e-5;
	auto expected_deaths = 12.5 + 14.3;
	ASSERT_FALSE(table.empty());
	ASSERT_EQ(2020, table.time_limits().lower());
	ASSERT_EQ(2022, table.time_limits().upper());
	ASSERT_EQ(1, table.age_limits().lower());
	ASSERT_EQ(15.0f, table.get_births_at(2021).number);
	ASSERT_EQ(0.85f, table.get_births_at(2021).sex_ratio);
	ASSERT_EQ(2, table.age_limits().upper());
	ASSERT_EQ(2, table.get_mortalities_at(2021).size());
	ASSERT_NEAR(expected_deaths, table.get_total_deaths_at(2021), tolerance);
}

TEST(TestHealthGPS_LifeTable, CreateEmptyBirthMismatchThrow)
{
	using namespace hgps;
	auto deaths = std::map<int, std::map<int, Mortality>>{};
	deaths.emplace(2021, std::map<int, Mortality>{ {1, Mortality(5.0f, 6.0f)} });

	ASSERT_THROW(LifeTable(std::map<int, Birth>{}, std::move(deaths)), std::invalid_argument);
}

TEST(TestHealthGPS_LifeTable, CreateEmptyDeathsMismatchThrow)
{
	using namespace hgps;
	auto births = std::map<int, Birth>{
		{2021, Birth(10.0f, 0.9f)}
	};
	
	ASSERT_THROW(LifeTable(std::move(births), std::map<int, std::map<int, Mortality>>{}),
		std::invalid_argument);
}

TEST(TestHealthGPS_LifeTable, CreateWithTimeRangeMismatchThrow)
{
	using namespace hgps;

	auto births = std::map<int, Birth>{
	{2020, Birth(10.0f, 0.9f)},
	{2021, Birth(15.0f, 0.9f)},
	};

	auto deaths = std::map<int, std::map<int, Mortality>>{};
	deaths.emplace(2021, std::map<int, Mortality>{ {1, Mortality(5.0f, 6.0f)} });

	ASSERT_THROW(LifeTable(std::map<int, Birth>{}, std::move(deaths)), std::invalid_argument);
}