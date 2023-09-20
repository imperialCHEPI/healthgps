#include "pch.h"

#include "HealthGPS.Core/interval.h"

TEST(TestCore_Interval, CreateEmpty) {
    using namespace hgps::core;
    auto empty = IntegerInterval{};

    ASSERT_EQ(0, empty.length());
    ASSERT_EQ(0, empty.lower());
    ASSERT_EQ(0, empty.upper());
    ASSERT_TRUE(empty.contains(0));
}

TEST(TestCore_Interval, CreatePositive) {
    using namespace hgps::core;
    auto lower = 0;
    auto upper = 10;
    auto len = upper - lower;
    auto mid = len / 2;
    auto animal = IntegerInterval{lower, upper};
    auto cat = IntegerInterval{mid, upper};
    auto dog = IntegerInterval{lower, mid};

    ASSERT_EQ(lower, animal.lower());
    ASSERT_EQ(upper, animal.upper());
    ASSERT_EQ(len, animal.length());
    ASSERT_TRUE(animal.contains(mid));
    ASSERT_TRUE(animal.contains(cat));
    ASSERT_TRUE(animal.contains(dog));
}

TEST(TestCore_Interval, CreateNegative) {
    using namespace hgps::core;
    auto lower = 0;
    auto upper = -10;
    auto len = upper - lower;
    auto mid = len / 2;
    auto animal = IntegerInterval{lower, upper};
    auto cat = IntegerInterval{mid, upper};
    auto dog = IntegerInterval{lower, mid};

    ASSERT_EQ(lower, animal.lower());
    ASSERT_EQ(upper, animal.upper());
    ASSERT_EQ(len, animal.length());
    ASSERT_TRUE(animal.contains(mid));
    ASSERT_TRUE(animal.contains(cat));
    ASSERT_TRUE(animal.contains(dog));
}

TEST(TestCore_Interval, Comparable) {
    using namespace hgps::core;

    auto lower = 0;
    auto upper = 10;
    auto len = upper - lower;
    auto mid = len / 2;

    auto cat = IntegerInterval{lower, upper};
    auto gato = IntegerInterval{lower, upper};
    auto dog = IntegerInterval{upper, upper + mid};
    auto cow = IntegerInterval{lower - mid, lower};

    ASSERT_EQ(cat, gato);
    ASSERT_GT(dog, cat);
    ASSERT_LT(cow, dog);

    ASSERT_TRUE(dog > cat);
    ASSERT_TRUE(cow < dog);
    ASSERT_TRUE(dog >= cat);
    ASSERT_TRUE(cow <= dog);

    ASSERT_TRUE(cat >= cow);
    ASSERT_TRUE(dog >= cat);
    ASSERT_FALSE(cat > dog);
    ASSERT_FALSE(dog < cow);
}

TEST(TestCore_Interval, ParseInteger) {
    using namespace hgps::core;
    auto lower = 0;
    auto upper = 10;

    auto animal = IntegerInterval{lower, upper};
    const auto *any_str = "TheFox";
    auto cat = parse_integer_interval(animal.to_string());
    ASSERT_EQ(animal, cat);
    ASSERT_THROW(parse_integer_interval(any_str), std::invalid_argument);
}

TEST(TestCore_Interval, ParseFloat) {
    using namespace hgps::core;
    auto lower = 0.0f;
    auto upper = 10.f;

    auto animal = FloatInterval{lower, upper};
    const auto *any_str = "TheFox";
    auto cat = parse_float_interval(animal.to_string());
    ASSERT_EQ(animal, cat);
    ASSERT_THROW(parse_float_interval(any_str), std::invalid_argument);
}

TEST(TestCore_Interval, ParseDouble) {
    using namespace hgps::core;
    auto lower = 0.0;
    auto upper = 10.0;

    auto animal = DoubleInterval{lower, upper};
    const auto *any_str = "TheFox";
    auto cat = parse_double_interval(animal.to_string());
    ASSERT_EQ(animal, cat);
    ASSERT_THROW(parse_double_interval(any_str), std::invalid_argument);
}
