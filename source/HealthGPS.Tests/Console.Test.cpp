#include "pch.h"
#include "..\HealthGPS.Console\include\program.h"

namespace hgpstest
{
	TEST(TestConsole, LocalTimeNow)
	{
		EXPECT_EQ(1, 1);
		EXPECT_TRUE(true);
		EXPECT_FALSE(hgps::getTimeNowStr().empty());
	}
}