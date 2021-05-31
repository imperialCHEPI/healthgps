#include "pch.h"
#include "..\HealthGPS\api.h"

TEST(TestHelathGPS, RandomBitGenerator)
{
	using namespace hgps;

	MTRandom32 rnd;
	EXPECT_EQ(RandomBitGenerator::min(), MTRandom32::min());
	EXPECT_EQ(RandomBitGenerator::max(), MTRandom32::max());
	EXPECT_TRUE(rnd() >= 0);
}

TEST(TestHelathGPS, SimulationInitialise)
{
	using namespace hgps;

	auto s = Scenario(2010, 2030);

	auto sim = Simulation(s);
	EXPECT_TRUE(sim.next_double() >= 0);
}