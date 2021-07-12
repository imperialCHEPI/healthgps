#include "pch.h"
#include <optional>

#include "HealthGPS.Core\univariate_summary.h"

TEST(TestCore_UnivariateSummary, CreateEmptyWithoutName)
{
	using namespace hgps::core;

	auto init_empty = UnivariateSummary();

	ASSERT_TRUE(init_empty.is_empty());

	EXPECT_EQ("Untitled", init_empty.name());
	EXPECT_TRUE(std::isnan(init_empty.min()));
	EXPECT_TRUE(std::isnan(init_empty.max()));
	EXPECT_EQ(0, init_empty.count_valid());
	EXPECT_EQ(0, init_empty.count_null());
	EXPECT_EQ(0, init_empty.count_total());
}

TEST(TestCore_UnivariateSummary, CreateEmptyWithName)
{
	using namespace hgps::core;

	auto init_name = UnivariateSummary("Test");

	ASSERT_TRUE(init_name.is_empty());

	EXPECT_EQ("Test", init_name.name());
	EXPECT_TRUE(std::isnan(init_name.min()));
	EXPECT_TRUE(std::isnan(init_name.max()));
	EXPECT_EQ(0, init_name.count_valid());
	EXPECT_EQ(0, init_name.count_null());
	EXPECT_EQ(0, init_name.count_total());
}

TEST(TestCore_UnivariateSummary, CreateFullFromVectorWithoutName)
{
	using namespace hgps::core;
	auto data = std::vector<double>{ 1.0, 2.0, 3.0, 4.0, 5.0 };

	auto init_vector = UnivariateSummary(data);

	ASSERT_FALSE(init_vector.is_empty());
	ASSERT_EQ("Untitled", init_vector.name());
	ASSERT_EQ(data.size(), init_vector.count_valid());
	ASSERT_EQ(0, init_vector.count_null());
	ASSERT_EQ(data.size(), init_vector.count_total());

	EXPECT_FALSE(std::isnan(init_vector.min()));
	EXPECT_FALSE(std::isnan(init_vector.max()));
}

TEST(TestCore_UnivariateSummary, CreateFullFromVectorWithName)
{
	using namespace hgps::core;
	auto data = std::vector<double>{ 1.0, 2.0, 3.0, 4.0, 5.0 };

	auto init_full = UnivariateSummary("Test", data);

	ASSERT_FALSE(init_full.is_empty());
	ASSERT_EQ("Test", init_full.name());
	ASSERT_EQ(data.size(), init_full.count_valid());
	ASSERT_EQ(0, init_full.count_null());
	ASSERT_EQ(data.size(), init_full.count_total());

	EXPECT_FALSE(std::isnan(init_full.min()));
	EXPECT_FALSE(std::isnan(init_full.max()));
}

TEST(TestCore_UnivariateSummary, CreateFullFromListWithName)
{
	using namespace hgps::core;
	auto data = std::vector<double>{ 1.0, 2.0, 3.0, 4.0, 5.0 };

	auto init_list = UnivariateSummary("Test", { 1.0, 2.0, 3.0, 4.0, 5.0 });

	ASSERT_FALSE(init_list.is_empty());
	ASSERT_EQ("Test", init_list.name());
	ASSERT_EQ(data.size(), init_list.count_valid());
	ASSERT_EQ(0, init_list.count_null());
	ASSERT_EQ(data.size(), init_list.count_total());

	EXPECT_FALSE(std::isnan(init_list.min()));
	EXPECT_FALSE(std::isnan(init_list.max()));
}

TEST(TestCore_UnivariateSummary, CreateFullValues)
{
	using namespace hgps::core;
	auto data = std::vector<double>{ 3.4, 0.5, 2.5, 12.6, 5.7, 8.3, 10.2, 15.8, 7.3, 9.7 };

	// min, max, range, sum, average, variance, std_dev, std_error, kurtosis, skewness
	auto expected = std::vector<double>{
	   0.5, 15.8, 15.3, 76.0, 7.6, 22.4066666666667, 4.73356806929685, 1.49688565584238,
	   -0.533980927444051, 0.172868169857032 };

	auto tolerance = 1e-10;
	auto summary = UnivariateSummary("Test", data);

	ASSERT_EQ("Test", summary.name());
	ASSERT_FALSE(summary.is_empty());
	ASSERT_EQ(0, summary.count_null());
	ASSERT_EQ(data.size(), summary.count_valid());
	ASSERT_EQ(data.size(), summary.count_total());
	ASSERT_DOUBLE_EQ(expected[0], summary.min());
	ASSERT_DOUBLE_EQ(expected[1], summary.max());
	ASSERT_DOUBLE_EQ(expected[2], summary.range());
	ASSERT_DOUBLE_EQ(expected[3], summary.sum());
	ASSERT_DOUBLE_EQ(expected[4], summary.average());
	EXPECT_NEAR(expected[5], summary.variance(), tolerance);
	EXPECT_NEAR(expected[6], summary.std_deviation(), tolerance);
	EXPECT_NEAR(expected[7], summary.std_error(), tolerance);
	EXPECT_NEAR(expected[8], summary.kurtosis(), tolerance);
	EXPECT_NEAR(expected[9], summary.skewness(), tolerance);
}

TEST(TestCore_UnivariateSummary, AppendOneValues)
{
	using namespace hgps::core;
	auto data = std::vector<double>{ 3.4, 0.5, 2.5, 12.6, 5.7, 8.3, 10.2, 15.8, 7.3, 9.7 };

	// min, max, range, sum, average, variance, std_dev, std_error, kurtosis, skewness
	auto expected = std::vector<double>{
	   0.5, 15.8, 15.3, 76.0, 7.6, 22.4066666666667, 4.73356806929685, 1.49688565584238,
	   -0.533980927444051, 0.172868169857032 };

	auto tolerance = 1e-10;
	auto summary = UnivariateSummary("Test");
	for (auto& v : data) {
		summary.append(v);
	}

	ASSERT_EQ("Test", summary.name());
	ASSERT_FALSE(summary.is_empty());
	ASSERT_EQ(0, summary.count_null());
	ASSERT_EQ(data.size(), summary.count_valid());
	ASSERT_EQ(data.size(), summary.count_total());
	ASSERT_DOUBLE_EQ(expected[0], summary.min());
	ASSERT_DOUBLE_EQ(expected[1], summary.max());
	ASSERT_DOUBLE_EQ(expected[2], summary.range());
	ASSERT_DOUBLE_EQ(expected[3], summary.sum());
	ASSERT_DOUBLE_EQ(expected[4], summary.average());
	EXPECT_NEAR(expected[5], summary.variance(), tolerance);
	EXPECT_NEAR(expected[6], summary.std_deviation(), tolerance);
	EXPECT_NEAR(expected[7], summary.std_error(), tolerance);
	EXPECT_NEAR(expected[8], summary.kurtosis(), tolerance);
	EXPECT_NEAR(expected[9], summary.skewness(), tolerance);
}

TEST(TestCore_UnivariateSummary, AppendRangeValues)
{
	using namespace hgps::core;
	auto data = std::vector<double>{ 3.4, 0.5, 2.5, 12.6, 5.7, 8.3, 10.2, 15.8, 7.3, 9.7 };

	// min, max, range, sum, average, variance, std_dev, std_error, kurtosis, skewness
	auto expected = std::vector<double>{
	   0.5, 15.8, 15.3, 76.0, 7.6, 22.4066666666667, 4.73356806929685, 1.49688565584238,
	   -0.533980927444051, 0.172868169857032 };

	auto tolerance = 1e-10;
	auto summary = UnivariateSummary("Test");
	summary.append(data);

	ASSERT_EQ("Test", summary.name());
	ASSERT_FALSE(summary.is_empty());
	ASSERT_EQ(0, summary.count_null());
	ASSERT_EQ(data.size(), summary.count_valid());
	ASSERT_EQ(data.size(), summary.count_total());
	ASSERT_DOUBLE_EQ(expected[0], summary.min());
	ASSERT_DOUBLE_EQ(expected[1], summary.max());
	ASSERT_DOUBLE_EQ(expected[2], summary.range());
	ASSERT_DOUBLE_EQ(expected[3], summary.sum());
	ASSERT_DOUBLE_EQ(expected[4], summary.average());
	EXPECT_NEAR(expected[5], summary.variance(), tolerance);
	EXPECT_NEAR(expected[6], summary.std_deviation(), tolerance);
	EXPECT_NEAR(expected[7], summary.std_error(), tolerance);
	EXPECT_NEAR(expected[8], summary.kurtosis(), tolerance);
	EXPECT_NEAR(expected[9], summary.skewness(), tolerance);
}

TEST(TestCore_UnivariateSummary, AppendNullValues)
{
	using namespace hgps::core;
	auto data = std::vector<double>{ 3.4, 0.5, 2.5, 12.6, 5.7, 8.3, 10.2, 15.8, 7.3, 9.7 };

	// min, max, range, sum, average, variance, std_dev, std_error, kurtosis, skewness
	auto expected = std::vector<double>{
	   0.5, 15.8, 15.3, 76.0, 7.6, 22.4066666666667, 4.73356806929685, 1.49688565584238,
	   -0.533980927444051, 0.172868169857032 };

	auto tolerance = 1e-10;
	auto summary = UnivariateSummary("Test", data);

	auto null_count = 4;
	auto null_value = std::optional<double>();
	summary.append_null();
	summary.append_null(2);
	summary.append(null_value);

	auto report = summary.to_string();
	ASSERT_TRUE(report.size() > 1);

	ASSERT_EQ("Test", summary.name());
	ASSERT_FALSE(summary.is_empty());
	ASSERT_EQ(null_count, summary.count_null());
	ASSERT_EQ(data.size(), summary.count_valid());
	ASSERT_EQ(data.size()+ null_count, summary.count_total());

	ASSERT_DOUBLE_EQ(expected[0], summary.min());
	ASSERT_DOUBLE_EQ(expected[1], summary.max());
	ASSERT_DOUBLE_EQ(expected[2], summary.range());
	ASSERT_DOUBLE_EQ(expected[3], summary.sum());
	ASSERT_DOUBLE_EQ(expected[4], summary.average());
	EXPECT_NEAR(expected[5], summary.variance(), tolerance);
	EXPECT_NEAR(expected[6], summary.std_deviation(), tolerance);
	EXPECT_NEAR(expected[7], summary.std_error(), tolerance);
	EXPECT_NEAR(expected[8], summary.kurtosis(), tolerance);
	EXPECT_NEAR(expected[9], summary.skewness(), tolerance);

	
}