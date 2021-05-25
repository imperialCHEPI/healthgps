#include "pch.h"
#include "..\HealthGPS\options.h"
TEST(TestHelathGPS, DefaultOptions)
{
	auto opt = hgps::ParseOptions();
	EXPECT_EQ(hgps::ParseOptions::Defaults().delimiter, opt.delimiter);
}



