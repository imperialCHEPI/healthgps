#include "pch.h"

#include "HealthGPS/data_series.h"

TEST(DataSeries, HasIncomeChannelReflectsCategorySetup) {
    using namespace hgps;
    using namespace hgps::core;

    DataSeries series(10);

    EXPECT_TRUE(series.has_income_category(Gender::male, Income::unknown));
    EXPECT_FALSE(series.has_income_channel(Gender::male, Income::unknown, "deaths"));
    EXPECT_FALSE(series.has_income_channel(Gender::female, Income::low, "count"));

    series.add_income_channels_for_categories({"deaths", "count"},
                                              {Income::unknown, Income::low});

    EXPECT_TRUE(series.has_income_channel(Gender::male, Income::unknown, "deaths"));
    EXPECT_TRUE(series.has_income_channel(Gender::female, Income::low, "count"));
    EXPECT_FALSE(series.has_income_channel(Gender::male, Income::middle, "count"));
    EXPECT_FALSE(series.has_income_channel(Gender::unknown, Income::low, "count"));
}

TEST(DataSeries, AddIncomeChannelsForCategoriesIsIdempotent) {
    using namespace hgps;
    using namespace hgps::core;

    DataSeries series(5);
    const std::vector<Income> strata{Income::high};

    series.add_income_channels_for_categories({"mean_yll"}, strata);
    series.at(Gender::male, Income::high, "mean_yll").at(2) = 3.5;
    series.add_income_channels_for_categories({"mean_yll"}, strata);

    EXPECT_DOUBLE_EQ(3.5, series.at(Gender::male, Income::high, "mean_yll").at(2));
    EXPECT_TRUE(series.has_income_channel(Gender::female, Income::high, "mean_yll"));
}

TEST(DataSeries, GetAvailableIncomeCategoriesListsPopulatedStrata) {
    using namespace hgps;
    using namespace hgps::core;

    DataSeries series(4);
    series.add_income_channels_for_categories({"count"}, {Income::lowermiddle});
    series.at(Gender::female, Income::lowermiddle, "count").at(1) = 1.0;

    const auto categories = series.get_available_income_categories();
    ASSERT_EQ(1u, categories.size());
    EXPECT_EQ(Income::lowermiddle, categories.front());
    EXPECT_EQ("LowerMiddleIncome", series.income_category_to_string(Income::lowermiddle));
}
