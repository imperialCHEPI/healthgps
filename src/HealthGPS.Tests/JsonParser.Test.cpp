#include "HealthGPS.Input/jsonparser.h"

#include "pch.h"

using namespace hgps::core;
using json = nlohmann::json;

TEST(JsonParser, IntervalFromJson) {
    {
        // Successful parsing of regular interval
        IntegerInterval interval;
        const auto j = json::array({0, 1});
        EXPECT_NO_THROW(j.get_to(interval));
        EXPECT_EQ(interval, IntegerInterval(0, 1));
    }

    {
        // Fail to parse empty array
        IntegerInterval interval;
        const auto j = json::array();
        EXPECT_THROW(j.get_to(interval), json::type_error);
    }

    {
        // Fail to parse array with > 2 elements
        IntegerInterval interval;
        const auto j = json::array({0, 1, 2});
        EXPECT_THROW(j.get_to(interval), json::type_error);
    }
}

TEST(JsonParser, IntervalToJson) {
    // Convert to JSON and back again
    const IntegerInterval interval{0, 1};
    const json j = interval;
    EXPECT_EQ(j.get<IntegerInterval>(), interval);
}
