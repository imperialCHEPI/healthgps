#include "pch.h"

#include "HealthGPS/gender_table.h"

TEST(TestHealthGPS_AgeGenderTable, CreateEmpty) {
    using namespace hgps;

    auto table = DoubleAgeGenderTable();

    ASSERT_EQ(0, table.size());
    ASSERT_EQ(0, table.rows());
    ASSERT_EQ(0, table.columns());
    ASSERT_TRUE(table.empty());
    ASSERT_FALSE(table.contains(0));
}

TEST(TestHealthGPS_AgeGenderTable, CreateBlank) {
    using namespace hgps;

    auto ages = std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto rows = MonotonicVector(ages);
    auto cols = std::vector<core::Gender>{core::Gender::male, core::Gender::female};
    auto data = core::DoubleArray2D(rows.size(), cols.size());
    auto table = DoubleAgeGenderTable(rows, cols, std::move(data));

    auto size = rows.size() * cols.size();
    ASSERT_EQ(size, table.size());
    ASSERT_EQ(rows.size(), table.rows());
    ASSERT_EQ(cols.size(), table.columns());
    ASSERT_FALSE(table.empty());
    ASSERT_TRUE(table.contains(0));
    ASSERT_TRUE(table.contains(5));
    ASSERT_TRUE(table.contains(10));
    ASSERT_FALSE(table.contains(13));
}

TEST(TestHealthGPS_AgeGenderTable, CreateWithSizeMismatchThrows) {
    using namespace hgps;

    auto ages = std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto rows = MonotonicVector(ages);
    auto cols = std::vector<core::Gender>{core::Gender::male, core::Gender::female};
    auto data_row = core::DoubleArray2D(rows.size() - 3, cols.size());
    auto data_col = core::DoubleArray2D(rows.size(), cols.size() - 1);
    auto data_all = core::DoubleArray2D(rows.size() - 3, cols.size() - 1);

    ASSERT_THROW(DoubleAgeGenderTable(rows, cols, std::move(data_row)), std::invalid_argument);
    ASSERT_THROW(DoubleAgeGenderTable(rows, cols, std::move(data_col)), std::invalid_argument);
    ASSERT_THROW(DoubleAgeGenderTable(rows, cols, std::move(data_all)), std::invalid_argument);
}

TEST(TestHealthGPS_AgeGenderTable, CreateBlankFunction) {
    using namespace hgps;

    auto age_range = core::IntegerInterval(0, 10);
    auto table = create_age_gender_table<double>(age_range);

    auto ncols = 2;
    auto nrows = age_range.length() + 1; // inclusive range
    auto size = nrows * ncols;
    ASSERT_EQ(size, table.size());
    ASSERT_EQ(nrows, table.rows());
    ASSERT_EQ(ncols, table.columns());
    ASSERT_FALSE(table.empty());
    ASSERT_TRUE(table.contains(0));
    ASSERT_TRUE(table.contains(5));
    ASSERT_TRUE(table.contains(10));
    ASSERT_FALSE(table.contains(13));
}

TEST(TestHealthGPS_AgeGenderTable, AccessContainsCheck) {
    using namespace hgps;

    auto age_range = core::IntegerInterval(0, 10);
    auto table = create_age_gender_table<double>(age_range);

    auto nrows = age_range.length() + 1;
    for (auto age = 0; age < nrows; age++) {
        ASSERT_TRUE(table.contains(age));
        ASSERT_TRUE(table.contains(age, core::Gender::male));
        ASSERT_TRUE(table.contains(age, core::Gender::female));
        ASSERT_FALSE(table.contains(13, core::Gender::male));
        ASSERT_FALSE(table.contains(13, core::Gender::female));
        ASSERT_FALSE(table.contains(age, core::Gender::unknown));
    }
}

TEST(TestHealthGPS_AgeGenderTable, AccessViaIndex) {
    using namespace hgps;

    auto age_range = core::IntegerInterval(0, 10);
    auto table = create_age_gender_table<double>(age_range);

    auto nrows = age_range.length() + 1;
    for (auto age = 0; age < nrows; age++) {
        ASSERT_EQ(0.0, table(age, core::Gender::male));
        ASSERT_EQ(0.0, table(age, core::Gender::female));

        ASSERT_EQ(0.0, table.at(age, core::Gender::male));
        ASSERT_EQ(0.0, table.at(age, core::Gender::female));
    }
}

TEST(TestHealthGPS_AgeGenderTable, AccessViaConstIndex) {
    using namespace hgps;

    auto age_range = core::IntegerInterval(0, 10);
    const auto table = create_age_gender_table<double>(age_range);

    auto nrows = age_range.length() + 1;
    for (auto age = 0; age < nrows; age++) {
        ASSERT_EQ(0.0, table(age, core::Gender::male));
        ASSERT_EQ(0.0, table(age, core::Gender::female));

        ASSERT_EQ(0.0, table.at(age, core::Gender::male));
        ASSERT_EQ(0.0, table.at(age, core::Gender::female));
    }
}

TEST(TestHealthGPS_AgeGenderTable, UpdateViaOperator) {
    using namespace hgps;

    auto age_range = core::IntegerInterval(0, 10);
    auto table = create_age_gender_table<double>(age_range);
    auto male_value = 5.0;
    auto female_value = 3.5;
    auto nrows = age_range.length() + 1;
    for (auto age = 0; age < nrows; age++) {
        table(age, core::Gender::male) = male_value + age;
        table(age, core::Gender::female) = female_value + age;
    }

    for (auto age = 0; age < nrows; age++) {
        ASSERT_EQ(male_value + age, table(age, core::Gender::male));
        ASSERT_EQ(female_value + age, table(age, core::Gender::female));
    }
}

TEST(TestHealthGPS_AgeGenderTable, UpdateViaFunction) {
    using namespace hgps;

    auto age_range = core::IntegerInterval(0, 10);
    auto table = create_age_gender_table<double>(age_range);
    auto male_value = 5.0;
    auto female_value = 3.5;
    auto nrows = age_range.length() + 1;
    for (auto age = 0; age < nrows; age++) {
        table.at(age, core::Gender::male) = male_value + age;
        table.at(age, core::Gender::female) = female_value + age;
    }

    for (auto age = 0; age < nrows; age++) {
        ASSERT_EQ(male_value + age, table.at(age, core::Gender::male));
        ASSERT_EQ(female_value + age, table.at(age, core::Gender::female));
    }
}

TEST(TestHealthGPS_AgeGenderTable, CreateWithWrongRangerThrows) {
    using namespace hgps;

    auto negative_range = core::IntegerInterval(-1, 10);
    auto inverted_range = core::IntegerInterval(1, 1);

    ASSERT_THROW(create_age_gender_table<double>(negative_range), std::invalid_argument);
    ASSERT_THROW(create_age_gender_table<double>(inverted_range), std::invalid_argument);
}

TEST(TestHealthGPS_AgeGenderTable, AccessEmptyThrows) {
    using namespace hgps;

    auto table = DoubleAgeGenderTable();

    ASSERT_THROW(table(0, core::Gender::male), std::out_of_range);
    ASSERT_THROW(table.at(0, core::Gender::male), std::out_of_range);
}

TEST(TestHealthGPS_AgeGenderTable, AccessOutOfRangeThrows) {
    using namespace hgps;

    auto age_range = core::IntegerInterval(0, 10);
    auto table = create_age_gender_table<double>(age_range);

    ASSERT_THROW(table(0, core::Gender::unknown), std::out_of_range);
    ASSERT_THROW(table(-1, core::Gender::female), std::out_of_range);
    ASSERT_THROW(table(11, core::Gender::female), std::out_of_range);

    ASSERT_THROW(table.at(0, core::Gender::unknown), std::out_of_range);
    ASSERT_THROW(table.at(-1, core::Gender::male), std::out_of_range);
    ASSERT_THROW(table.at(11, core::Gender::male), std::out_of_range);
}
