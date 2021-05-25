#include <sstream>
#include "pch.h"

#include "..\HealthGPS.Core\version.h"

TEST(TestHealthGPSCore, CurrentApiVersion)
{
	using namespace hgps::core;

	std::stringstream ss;
	ss << API_MAJOR << "." << API_MINOR << "." << API_PATCH;	

	EXPECT_EQ(API_MAJOR, Version::GetMajor());
	EXPECT_EQ(API_MINOR, Version::GetMinor());
	EXPECT_EQ(API_PATCH, Version::GetPatch());
	EXPECT_EQ(ss.str(), Version::GetVersion());

	EXPECT_TRUE(Version::HasFeature("COUNTRY"));
	EXPECT_FALSE(Version::HasFeature("DISEASES"));

	EXPECT_TRUE(Version::IsAtLeast(API_MAJOR, API_MINOR, API_PATCH));
	EXPECT_FALSE(Version::IsAtLeast(API_MAJOR + 1,0, 0));
	EXPECT_FALSE(Version::IsAtLeast(API_MAJOR, API_MINOR + 1, API_PATCH));
	EXPECT_FALSE(Version::IsAtLeast(API_MAJOR, API_MINOR, API_PATCH + 1));
}

