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
	auto gb_lower = manager.get_country("gb");
	auto gb_upper = manager.get_country("GB");

	ASSERT_GT(countries.size(), 0);
	ASSERT_TRUE(gb_lower.has_value());
	ASSERT_TRUE(gb_upper.has_value());
}
