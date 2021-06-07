#include "pch.h"
#include "HealthGPS.Datastore\api.h"

namespace fs = std::filesystem;

TEST(TestDatastore, CreateDataManager)
{
	using namespace hgps::data;

	auto full_path = fs::absolute("../../../data");

	auto manager = DataManager(full_path);

	auto countries = manager.get_countries();

	ASSERT_GT(countries.size(), 0);
}