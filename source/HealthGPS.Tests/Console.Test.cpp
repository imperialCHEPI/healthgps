#include "pch.h"
#include "..\HealthGPS.Console\include\program.h"

TEST(TestConsole, LocalTimeNow)
{
	using namespace hgps::host;

	EXPECT_FALSE(getTimeNowStr().empty());
}
