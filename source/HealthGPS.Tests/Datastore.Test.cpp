#include "pch.h"
#include "HealthGPS.Datastore\api.h"

namespace fs = std::filesystem;

static auto store_full_path = fs::absolute("../../../data");
TEST(TestDatastore, CreateDataManager)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto countries = manager.get_countries();

	ASSERT_GT(countries.size(), 0);
}

TEST(TestDatastore, CountryIsCaseInsensitive)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto countries = manager.get_countries();
	auto gb2_lower = manager.get_country("gb");
	auto gb2_upper = manager.get_country("GB");
	auto gb3_lower = manager.get_country("gbr");
	auto gb3_upper = manager.get_country("GBR");

	ASSERT_GT(countries.size(), 0);
	ASSERT_TRUE(gb2_lower.has_value());
	ASSERT_TRUE(gb2_upper.has_value());
	ASSERT_TRUE(gb3_lower.has_value());
	ASSERT_TRUE(gb3_upper.has_value());
}

TEST(TestDatastore, CountryPopulation)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);
	auto uk = manager.get_country("GB");
	auto uk_pop = manager.get_population(uk.value());

	// Filter out the ends of the years range
	auto uk_pop_min = uk_pop.front().year;
	auto uk_pop_max = uk_pop.back().year;
	auto mid_year = std::midpoint(uk_pop_min, uk_pop_max);
	auto uk_pop_flt = manager.get_population(uk.value(), [&mid_year](const int& value)
		{return value >= (mid_year-1) && value <= (mid_year+1); });

	ASSERT_GT(uk_pop.size(), 0);
	ASSERT_GT(uk_pop_flt.size(), 0);

	ASSERT_GT(uk_pop.size(), uk_pop_flt.size());
	ASSERT_LT(uk_pop_min, uk_pop_flt.front().year);
	ASSERT_GT(uk_pop_max, uk_pop_flt.back().year);
}