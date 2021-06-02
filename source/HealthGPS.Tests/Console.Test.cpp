#include "pch.h"
#include "..\HealthGPS.Console\program.h"

TEST(TestConsole, LocalTimeNow)
{
	using namespace hgps::host;

	EXPECT_FALSE(getTimeNowStr().empty());
}
