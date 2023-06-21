#include "pch.h"

#include "HealthGPS/gender_value.h"
#include "HealthGPS/map2d.h"

using IntegerMap2d = hgps::Map2d<int, int, int>;
using TimeAgeMap2d = hgps::Map2d<int, int, hgps::DoubleGenderValue>;

TEST(TestHealthGPS_Map2d, CreateEmpty) {
    using namespace hgps;

    auto m = IntegerMap2d(std::map<int, std::map<int, int>>{});

    ASSERT_TRUE(m.empty());
    ASSERT_EQ(0, m.rows());
    ASSERT_EQ(0, m.columns());
    ASSERT_FALSE(m.contains(1));
}

TEST(TestHealthGPS_Map2d, CreateWithBasicType) {
    using namespace hgps;
    auto data = std::map<int, std::map<int, int>>{
        {2019, {{1, 5}, {2, 6}, {3, 7}}},
        {2020, {{1, 7}, {2, 9}, {3, 11}}},
        {2021, {{1, 2}, {2, 6}, {3, 10}}},
        {2022, {{1, 3}, {2, 7}, {3, 9}}},
    };

    auto m = IntegerMap2d(std::move(data));

    ASSERT_FALSE(m.empty());
    ASSERT_EQ(4, m.rows());
    ASSERT_EQ(3, m.columns());
    ASSERT_EQ(12, m.size());
    ASSERT_TRUE(m.contains(2021));
}

TEST(TestHealthGPS_Map2d, CreateWithUserType) {
    using namespace hgps;
    auto data = std::map<int, std::map<int, DoubleGenderValue>>{
        {2019, {{1, DoubleGenderValue{5, 10}}, {2, DoubleGenderValue{6, 18}}}},
        {2020, {{1, DoubleGenderValue{7, 14}}, {2, DoubleGenderValue{9, 27}}}},
        {2021, {{1, DoubleGenderValue{2, 4}}, {2, DoubleGenderValue{6, 24}}}},
        {2022, {{1, DoubleGenderValue{3, 6}}, {2, DoubleGenderValue{7, 21}}}},
    };

    auto m = TimeAgeMap2d(std::move(data));

    ASSERT_FALSE(m.empty());
    ASSERT_EQ(4, m.rows());
    ASSERT_EQ(2, m.columns());
    ASSERT_EQ(8, m.size());
    ASSERT_TRUE(m.contains(2021));
    ASSERT_TRUE(m.contains(2021, 2));
    ASSERT_EQ(2, m.at(2021, 1).males);
    ASSERT_EQ(4, m.at(2021, 1).females);
}

TEST(TestHealthGPS_Map2d, IterateOverRows) {
    using namespace hgps;
    auto data = std::map<int, std::map<int, DoubleGenderValue>>{
        {2019, {{1, DoubleGenderValue{5, 10}}, {2, DoubleGenderValue{6, 18}}}},
        {2020, {{1, DoubleGenderValue{7, 14}}, {2, DoubleGenderValue{9, 27}}}},
        {2021, {{1, DoubleGenderValue{2, 4}}, {2, DoubleGenderValue{6, 24}}}},
        {2022, {{1, DoubleGenderValue{3, 6}}, {2, DoubleGenderValue{7, 21}}}},
    };

    auto m = TimeAgeMap2d(std::move(data));
    for (auto &row : m) {

        ASSERT_FALSE(row.second.empty());
        ASSERT_EQ(2, row.second.size());
    }
}

TEST(TestHealthGPS_Map2d, AccessSingleRows) {
    using namespace hgps;
    auto data = std::map<int, std::map<int, DoubleGenderValue>>{
        {2019, {{1, DoubleGenderValue{5, 10}}, {2, DoubleGenderValue{6, 18}}}},
        {2020, {{1, DoubleGenderValue{7, 14}}, {2, DoubleGenderValue{9, 27}}}},
        {2021, {{1, DoubleGenderValue{2, 4}}, {2, DoubleGenderValue{6, 24}}}},
        {2022, {{1, DoubleGenderValue{3, 6}}, {2, DoubleGenderValue{7, 21}}}},
    };

    auto m = TimeAgeMap2d(std::move(data));
    auto r = m.row(2022);

    ASSERT_FALSE(r.empty());
    ASSERT_EQ(2, r.size());
    ASSERT_EQ(3, r.at(1).males);
    ASSERT_EQ(6, r.at(1).females);
    ASSERT_EQ(7, r.at(2).males);
    ASSERT_EQ(21, r.at(2).females);
}

TEST(TestHealthGPS_Map2d, AccessSingleCell) {
    using namespace hgps;
    auto data = std::map<int, std::map<int, DoubleGenderValue>>{
        {2019, {{1, DoubleGenderValue{5, 10}}, {2, DoubleGenderValue{6, 18}}}},
        {2020, {{1, DoubleGenderValue{7, 14}}, {2, DoubleGenderValue{9, 27}}}},
        {2021, {{1, DoubleGenderValue{2, 4}}, {2, DoubleGenderValue{6, 24}}}},
        {2022, {{1, DoubleGenderValue{3, 6}}, {2, DoubleGenderValue{7, 21}}}},
    };

    auto m = TimeAgeMap2d(std::move(data));
    ASSERT_EQ(5, m.at(2019, 1).males);
    ASSERT_EQ(6, m.at(2022, 1).females);
    ASSERT_EQ(9, m.at(2020, 2).males);
    ASSERT_EQ(21, m.at(2022, 2).females);
}