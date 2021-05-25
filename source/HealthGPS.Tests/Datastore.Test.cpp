#include "pch.h"
#include "..\HealthGPS.Datastore\options.h"

TEST(TestDatastore, DefaultOptions)
{
	using namespace hgps::data;

	auto opt = ReadOptions();
	EXPECT_EQ(ReadOptions::Defaults().block_size, opt.block_size);
}