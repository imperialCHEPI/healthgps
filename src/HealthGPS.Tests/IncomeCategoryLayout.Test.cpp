#include "pch.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/income_category_layout.h"

TEST(IncomeCategoryLayout, ParsesThreeFourAndFive) {
    using namespace hgps::core;

    const auto layout3 = income_category_layout_from_config("3");
    EXPECT_EQ(3u, layout3.count);
    EXPECT_EQ(3u, layout3.strata.size());

    const auto layout4 = income_category_layout_from_config("4");
    EXPECT_EQ(4u, layout4.count);
    EXPECT_EQ(Income::lowermiddle, layout4.strata[1]);

    const auto layout5 = income_category_layout_from_config("5");
    EXPECT_EQ(5u, layout5.count);
    EXPECT_EQ(Income::middle, layout5.strata[2]);
}

TEST(IncomeCategoryLayout, RejectsInvalidCategoryCount) {
    EXPECT_THROW(hgps::core::income_category_layout_from_config("2"), hgps::core::HgpsException);
}

TEST(IncomeCategoryLayout, TableIndexAndBucketMapping) {
    using namespace hgps::core;

    const auto layout5 = income_category_layout_from_config("5");
    EXPECT_EQ(0u, income_table_index(Income::low, layout5));
    EXPECT_EQ(4u, income_table_index(Income::high, layout5));
    EXPECT_EQ(Income::uppermiddle, income_from_equal_split_bucket(3u, layout5));
}

TEST(IncomeCategoryLayout, NumericEncodingPreservesLegacyAndSupportsFive) {
    using namespace hgps::core;

    const auto layout3 = income_category_layout_from_config("3");
    EXPECT_DOUBLE_EQ(1.0, income_category_numeric(Income::low, layout3));
    EXPECT_DOUBLE_EQ(2.0, income_category_numeric(Income::middle, layout3));
    EXPECT_DOUBLE_EQ(4.0, income_category_numeric(Income::high, layout3));

    const auto layout4 = income_category_layout_from_config("4");
    EXPECT_DOUBLE_EQ(3.0, income_category_numeric(Income::uppermiddle, layout4));

    const auto layout5 = income_category_layout_from_config("5");
    EXPECT_DOUBLE_EQ(1.0, income_category_numeric(Income::low, layout5));
    EXPECT_DOUBLE_EQ(5.0, income_category_numeric(Income::high, layout5));
}
