#include "pch.h"
#include "HealthGPS.Datastore\api.h"

namespace fs = std::filesystem;

static auto store_full_path = fs::absolute("../../../data");
TEST(TestDatastore, CreateDataManager)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto countries = manager.get_countries();

	ASSERT_GT(countries.size(), 0);
}

TEST(TestDatastore, CountryIsCaseInsensitive)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto countries = manager.get_countries();
	auto gb2_lower = manager.get_country("gb");
	auto gb2_upper = manager.get_country("GB");
	auto gb3_lower = manager.get_country("gbr");
	auto gb3_upper = manager.get_country("GBR");

	ASSERT_GT(countries.size(), 0);
	ASSERT_TRUE(gb2_lower.has_value());
	ASSERT_TRUE(gb2_upper.has_value());
	ASSERT_TRUE(gb3_lower.has_value());
	ASSERT_TRUE(gb3_upper.has_value());
}

TEST(TestDatastore, CountryPopulation)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);
	auto uk = manager.get_country("GB");
	auto uk_pop = manager.get_population(uk.value());

	// Filter out the ends of the years range
	auto uk_pop_min = uk_pop.front().year;
	auto uk_pop_max = uk_pop.back().year;
	auto mid_year = std::midpoint(uk_pop_min, uk_pop_max);
	auto uk_pop_flt = manager.get_population(uk.value(), [&mid_year](const int& value)
		{return value >= (mid_year-1) && value <= (mid_year+1); });

	ASSERT_GT(uk_pop.size(), 0);
	ASSERT_GT(uk_pop_flt.size(), 0);

	ASSERT_GT(uk_pop.size(), uk_pop_flt.size());
	ASSERT_LT(uk_pop_min, uk_pop_flt.front().year);
	ASSERT_GT(uk_pop_max, uk_pop_flt.back().year);
}

TEST(TestDatastore, RetrieveDeseasesInfo)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto diseases = manager.get_diseases();
	for (auto& item : diseases)	{
		auto info = manager.get_disease_info(item.code);
		EXPECT_TRUE(info.has_value());
		EXPECT_EQ(item.code, info.value().code);
	}

	ASSERT_GT(diseases.size(), 0);
}

TEST(TestDatastore, RetrieveDeseaseDefinition)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto uk = manager.get_country("GB").value();
	auto diseases = manager.get_diseases();
	for (auto& item : diseases) {
		auto entity = manager.get_disease(item, uk);

		ASSERT_GT(entity.measures.size(), 0);
		ASSERT_GT(entity.items.size(), 0);
		EXPECT_EQ(item.code, entity.info.code);
		EXPECT_EQ(uk.code, entity.country.code);
	}
}

TEST(TestDatastore, DiseaseRelativeRiskToDisease)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto backpain = manager.get_disease_info("lowbackpain").value();
	auto diabetes = manager.get_disease_info("diabetes").value();

	auto table_self = manager.get_relative_risk_to_disease(diabetes, diabetes);
	auto table_other = manager.get_relative_risk_to_disease(diabetes, backpain);

	ASSERT_EQ(3, table_self.columns.size());
	ASSERT_GT(table_self.rows.size(), 0);
	ASSERT_EQ(table_self.rows[0][0], table_self.rows[0][1]);
	ASSERT_FALSE(table_self.is_default_value);

	ASSERT_EQ(3, table_other.columns.size());
	ASSERT_GT(table_other.rows.size(), 0);
	ASSERT_NE(table_other.rows[0][0], table_other.rows[0][1]);
	ASSERT_FALSE(table_other.is_default_value);
}

TEST(TestDatastore, DiseaseRelativeRiskToRiskFactor)
{
	using namespace hgps;
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto risk_factor = "bmi";
	auto diabetes = manager.get_disease_info("diabetes").value();

	auto col_size = 6;

	auto table_male = manager.get_relative_risk_to_risk_factor(diabetes, core::Gender::male, risk_factor);
	auto table_feme = manager.get_relative_risk_to_risk_factor(diabetes, core::Gender::female, risk_factor);

	ASSERT_EQ(col_size, table_male.columns.size());
	ASSERT_GT(table_male.rows.size(), 0);
	ASSERT_NE(table_male.rows[0][0], table_male.rows[0][1]);
	ASSERT_FALSE(table_male.is_default_value);

	ASSERT_EQ(col_size, table_feme.columns.size());
	ASSERT_GT(table_feme.rows.size(), 0);
	ASSERT_NE(table_feme.rows[0][0], table_feme.rows[0][1]);
	ASSERT_FALSE(table_feme.is_default_value);
}