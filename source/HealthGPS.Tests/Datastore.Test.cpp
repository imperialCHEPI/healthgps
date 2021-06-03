#include "pch.h"
#include "HealthGPS.Datastore\options.h"

TEST(TestDatastore, DefaultOptions)
{
	using namespace hgps::data;

	auto opt = ReadOptions();
	EXPECT_EQ(ReadOptions::Defaults().block_size, opt.block_size);
}

TEST(TestDatastore, CoreAPIVersion)
{
	using namespace hgps::data;

	auto version = ReadOptions::GetApiVersion();
	EXPECT_EQ("0.1.0", version);
}