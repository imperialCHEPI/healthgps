#include "pch.h"
#include "HealthGPS.Datastore\api.h"

namespace fs = std::filesystem;

TEST(TestDatastore, CreateFileRepository)
{
	using namespace hgps::data;

	auto full_path = fs::absolute("../../../data");

	auto repo = FileRepository(full_path);

	auto countries = repo.get_countries();

	ASSERT_GT(countries.size(), 0);
}

TEST(TestDatastore, CreateDataManager)
{
	using namespace hgps::data;

	auto full_path = fs::absolute("../../../data");

	auto repo = FileRepository(full_path);

	auto manager = DataManager(repo);

	auto countries = manager.get_countries();

	ASSERT_GT(countries.size(), 0);
}