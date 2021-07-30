#include "pch.h"

#include "HealthGPS\disease.h"
#include "HealthGPS\disease_table.h"
#include "HealthGPS\relative_risk.h"
#include "HealthGPS\diabetes_model.h"
TEST(TestHealthGPS_Disease, CreateDiseaseMeasure)
{
	using namespace hgps;

	auto measure = DiseaseMeasure(std::map<int, double>{ {1, 3.14}, { 2, 2.13 }});
	ASSERT_EQ(2, measure.size());
	ASSERT_EQ(3.14, measure[1]);
	ASSERT_EQ(3.14, measure.at(1));
	ASSERT_EQ(2.13, measure[2]);
	ASSERT_EQ(2.13, measure.at(2));
}

TEST(TestHealthGPS_Disease, AccessDiseaseMeasureOutOfBoundThrows)
{
	using namespace hgps;

	auto measure = DiseaseMeasure(std::map<int, double>{ {1, 3.14}, { 2, 2.13 }});

	ASSERT_EQ(2, measure.size());
	ASSERT_THROW(measure[3], std::out_of_range);
	ASSERT_THROW(measure.at(3), std::out_of_range);
}

TEST(TestHealthGPS_Disease, CreateDiseaseTable)
{
	using namespace hgps;

	auto measures = std::map<std::string, int>{ {"x",1},{"y",2} };
	auto data = std::map<int, std::map<core::Gender, DiseaseMeasure>>{
		{1,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.14}, {2, 2.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 5.13}, {2, 3.13}})}} },

		{2,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.33}, {2, 3.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 3.77}, {2, 7.13}})}} },

		{3,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.22}, {2, 2.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 7.13}, {2, 3.13}})}} }
	};

	auto info = core::DiseaseInfo{ .code = "Test", .name = "Test disease." };

	auto table = DiseaseTable(info, std::move(measures), std::move(data));
	ASSERT_EQ(info.code, table.info().code);
	ASSERT_EQ(2, table.cols());
	ASSERT_EQ(3, table.rows());
	ASSERT_EQ(6, table.size());
	ASSERT_EQ(2, table.measures().size());
}

TEST(TestHealthGPS_Disease, CreateDiseaseTableEmpty)
{
	using namespace hgps;

	auto measures = std::map<std::string, int>();
	auto data = std::map<int, std::map<core::Gender, DiseaseMeasure>>();
	auto info = core::DiseaseInfo{ .code = "Test", .name = "Test disease." };

	auto table = DiseaseTable(info, std::move(measures), std::move(data));
	ASSERT_EQ(info.code, table.info().code);
	ASSERT_EQ(0, table.cols());
	ASSERT_EQ(0, table.rows());
	ASSERT_EQ(0, table.size());
	ASSERT_EQ(0, table.measures().size());
}

TEST(TestHealthGPS_Disease, AccessDiseaseTable)
{
	using namespace hgps;

	auto measures = std::map<std::string, int>{ {"x",1},{"y",2} };
	auto data = std::map<int, std::map<core::Gender, DiseaseMeasure>>{
		{1,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.14}, {2, 2.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 5.13}, {2, 3.13}})}} },

		{2,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.33}, {2, 3.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 3.77}, {2, 7.13}})}} },

		{3,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.22}, {2, 2.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 7.13}, {2, 3.13}})}} }
	};

	auto info = core::DiseaseInfo{ .code = "Test", .name = "Test disease." };
	auto table = DiseaseTable(info, std::move(measures), std::move(data));

	auto mapping = table.measures();

	auto age1_male = table(1, core::Gender::male);
	auto age1_male_x = table(1, core::Gender::male)[1];
	auto age1_male_y = table(1, core::Gender::male)[table["y"]];

	auto age3_female = table(3, core::Gender::female);
	auto age3_female_x = table(3, core::Gender::female).at(table["x"]);
	auto age3_female_y = table(3, core::Gender::female).at(2);

	const auto age2_male = table(2, core::Gender::male);
	const auto age2_male_x = table(2, core::Gender::male)[1];
	const auto age2_male_y = table(2, core::Gender::male).at(2);

	ASSERT_EQ(2, mapping.size());

	ASSERT_EQ(3.14, age1_male[1]);
	ASSERT_EQ(2.13, age1_male[2]);
	ASSERT_EQ(age1_male[1], age1_male_x);
	ASSERT_EQ(age1_male[2], age1_male_y);

	ASSERT_EQ(7.13, age3_female[1]);
	ASSERT_EQ(3.13, age3_female[2]);
	ASSERT_EQ(age3_female[1], age3_female_x);
	ASSERT_EQ(age3_female[2], age3_female_y);

	ASSERT_EQ(3.33, age2_male[1]);
	ASSERT_EQ(3.13, age2_male[2]);
	ASSERT_EQ(age2_male[1], age2_male_x);
	ASSERT_EQ(age2_male[2], age2_male_y);
}

TEST(TestHealthGPS_Disease, AccessDiseaseTableOutOfBoundThrows)
{
	using namespace hgps;

	auto measures = std::map<std::string, int>{ {"x",1},{"y",2} };
	auto data = std::map<int, std::map<core::Gender, DiseaseMeasure>>{
		{1,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.14}, {2, 2.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 5.13}, {2, 3.13}})}} },

		{2,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.33}, {2, 3.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 3.77}, {2, 7.13}})}} },

		{3,{{core::Gender::male, DiseaseMeasure(std::map<int, double>{ {1, 3.22}, {2, 2.13}})},
			{core::Gender::female, DiseaseMeasure(std::map<int, double>{ {1, 7.13}, {2, 3.13}})}} }
	};

	auto info = core::DiseaseInfo{ .code = "Test", .name = "Test disease." };
	auto table = DiseaseTable(info, std::move(measures), std::move(data));

	// Measures
	ASSERT_THROW(table["z"], std::out_of_range);
	ASSERT_THROW(table.at("z"), std::out_of_range);

	// Values
	ASSERT_THROW(table(0, core::Gender::male), std::out_of_range);
	ASSERT_THROW(table(2, core::Gender::unknown), std::out_of_range);
}

TEST(TestHealthGPS_Disease, CreateMonotonicVector)
{
	using namespace hgps;

	auto data = std::vector<int>{ 1, 3, 5, 7, 8, 9, 11, 13 };
	std::vector<int> reverse(data.size());
	std::reverse_copy(data.begin(), data.end(), reverse.begin());

	auto increasing = MonotonicVector(data);
	auto decreasing = MonotonicVector(reverse);

	ASSERT_EQ(data.size(), increasing.size());
	ASSERT_EQ(reverse.size(), decreasing.size());
}

TEST(TestHealthGPS_Disease, MonotonicVectorThrowsForDuplicate)
{
	using namespace hgps;

	auto data = std::vector<int>{ 1, 3, 3, 7, 8, 9, 11, 13 };

	ASSERT_THROW(auto test = MonotonicVector(data), std::invalid_argument);
}

TEST(TestHealthGPS_Disease, MonotonicVectorThrowsForUnodered)
{
	using namespace hgps;

	auto data = std::vector<int>{ 1, 5, 3, 7, 8, 9, 11, 13 };

	ASSERT_THROW(auto test = MonotonicVector(data), std::invalid_argument);
}

TEST(TestHealthGPS_Disease, AccessMonotonicVectorIndex)
{
	using namespace hgps;
	auto data = std::vector<int>{ 1, 3, 5, 7, 8, 9, 11, 13 };
	auto test = MonotonicVector(data);
	for (size_t i = 0; i < data.size(); i++) {
		ASSERT_EQ(data[i], test[i]);
		ASSERT_EQ(data.at(i), test.at(i));
	}
}

TEST(TestHealthGPS_Disease, AccessMonotonicVectorIterator)
{
	using namespace hgps;
	auto data = std::vector<int>{ 1, 3, 5, 7, 8, 9, 11, 13 };
	auto test = MonotonicVector(data);
	const auto const_vec = MonotonicVector(data);

	const auto [min, max] = std::minmax_element(data.begin(), data.end());
	for (auto& v : test) {
		ASSERT_GE(v, *min);
		ASSERT_LE(v, *max);
	}

	for (const auto& v : const_vec) {
		ASSERT_GE(v, *min);
		ASSERT_LE(v, *max);
	}
}

TEST(TestHealthGPS_Disease, AccessMonotonicVectorThrowsOutOfbound)
{
	using namespace hgps;

	auto data = std::vector<int>{ 1, 3, 5, 7, 8, 9, 11, 13 };

	auto test = MonotonicVector(data);

	ASSERT_THROW(test[data.size()], std::out_of_range);
	ASSERT_THROW(test.at(data.size()), std::out_of_range);
}
