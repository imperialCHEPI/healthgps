#include "pch.h"
#include "data_config.h"

#include "HealthGPS.Datastore/api.h"

namespace fs = std::filesystem;

static auto store_full_path = default_datastore_path();

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
		{return value >= (mid_year - 1) && value <= (mid_year + 1); });

	ASSERT_GT(uk_pop.size(), 0);
	ASSERT_GT(uk_pop_flt.size(), 0);

	ASSERT_GT(uk_pop.size(), uk_pop_flt.size());
	ASSERT_LT(uk_pop_min, uk_pop_flt.front().year);
	ASSERT_GT(uk_pop_max, uk_pop_flt.back().year);
}

TEST(TestDatastore, CountryMortality)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);
	auto uk = manager.get_country("GB");
	auto uk_deaths = manager.get_mortality(uk.value());

	// Filter out the ends of the years range
	auto uk_deaths_min = uk_deaths.front().year;
	auto uk_deaths_max = uk_deaths.back().year;
	auto mid_year = std::midpoint(uk_deaths_min, uk_deaths_max);
	auto uk_deaths_flt = manager.get_mortality(uk.value(), [&mid_year](const int& value)
		{return value >= (mid_year - 1) && value <= (mid_year + 1); });

	ASSERT_GT(uk_deaths.size(), 0);
	ASSERT_GT(uk_deaths_flt.size(), 0);
	ASSERT_GT(uk_deaths_max, uk_deaths_min);

	ASSERT_GT(uk_deaths.size(), uk_deaths_flt.size());
	ASSERT_LT(uk_deaths_min, uk_deaths_flt.front().year);
	ASSERT_GT(uk_deaths_max, uk_deaths_flt.back().year);
}

TEST(TestDatastore, RetrieveDeseasesInfo)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto diseases = manager.get_diseases();
	for (auto& item : diseases) {
		auto info = manager.get_disease_info(item.code);
		EXPECT_TRUE(info.has_value());
		EXPECT_EQ(item.code, info.value().code);
	}

	ASSERT_GT(diseases.size(), 0);
}

TEST(TestDatastore, RetrieveDeseasesTypeInInfo)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto diseases = manager.get_diseases();
	auto cancer_count = 0;
	for (auto& item : diseases) {
		if (item.group == DiseaseGroup::cancer) {
			cancer_count++;
		}
	}

	ASSERT_GT(diseases.size(), 0);
	ASSERT_GT(cancer_count, 0);
}

TEST(TestDatastore, RetrieveDeseasesInfoHasNoValue)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto info = manager.get_disease_info("ghost369");
	EXPECT_FALSE(info.has_value());
}

TEST(TestDatastore, RetrieveDeseaseDefinition)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto uk = manager.get_country("GB").value();
	auto diseases = manager.get_diseases();
	for (auto& item : diseases) {
		auto entity = manager.get_disease(item, uk);

		ASSERT_FALSE(entity.empty());
		ASSERT_GT(entity.measures.size(), 0);
		ASSERT_GT(entity.items.size(), 0);
		EXPECT_EQ(item.code, entity.info.code);
		EXPECT_EQ(uk.code, entity.country.code);
	}
}

TEST(TestDatastore, RetrieveDeseaseDefinitionIsEmpty)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto uk = manager.get_country("GB").value();
	auto info = DiseaseInfo{ .group = DiseaseGroup::other, .code = "ghost369", .name = "Look at the flowers." };
	auto entity = manager.get_disease(info, uk);

	ASSERT_TRUE(entity.empty());
	ASSERT_EQ(0, entity.items.size());
	ASSERT_EQ(0, entity.measures.size());
	EXPECT_EQ(info.code, entity.info.code);
	EXPECT_EQ(uk.code, entity.country.code);
}

TEST(TestDatastore, DiseaseRelativeRiskToDisease)
{
	using namespace hgps;
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto asthma = manager.get_disease_info("asthma").value();
	auto diabetes = manager.get_disease_info("diabetes").value();

	auto table_self = manager.get_relative_risk_to_disease(diabetes, diabetes);
	auto table_other = manager.get_relative_risk_to_disease(diabetes, asthma);

	ASSERT_EQ(3, table_self.columns.size());
	ASSERT_GT(table_self.rows.size(), 0);
	ASSERT_EQ(table_self.rows[0][1], table_self.rows[0][2]);
	ASSERT_FALSE(table_self.is_default_value);

	ASSERT_EQ(3, table_other.columns.size());
	ASSERT_GT(table_other.rows.size(), 0);
	ASSERT_EQ(table_other.rows[0][1], table_other.rows[0][2]);
	ASSERT_FALSE(table_other.is_default_value);
}

TEST(TestDatastore, DefaultDiseaseRelativeRiskToDisease)
{
	using namespace hgps;
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);
	auto diabetes = manager.get_disease_info("diabetes").value();
	auto ghost_disease = DiseaseInfo{ .group = DiseaseGroup::other, .code = "ghost369", .name = "Look at the flowers." };
	auto default_table = manager.get_relative_risk_to_disease(diabetes, ghost_disease);

	ASSERT_EQ(3, default_table.columns.size());
	ASSERT_GT(default_table.rows.size(), 0);
	ASSERT_EQ(1.0, default_table.rows[0][1]);
	ASSERT_EQ(1.0, default_table.rows[0][2]);
	ASSERT_TRUE(default_table.is_default_value);
}

TEST(TestDatastore, DiseaseRelativeRiskToRiskFactor)
{
	using namespace hgps;
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto risk_factor = "bmi";
	auto diabetes = manager.get_disease_info("diabetes").value();

	auto col_size = 8;

	auto table_male = manager.get_relative_risk_to_risk_factor(diabetes, core::Gender::male, risk_factor);
	auto table_feme = manager.get_relative_risk_to_risk_factor(diabetes, core::Gender::female, risk_factor);

	ASSERT_EQ(col_size, table_male.columns.size());
	ASSERT_GT(table_male.rows.size(), 0);
	ASSERT_EQ(table_male.rows[0][1], table_male.rows[0][2]);
	ASSERT_NE(table_male.rows[0][1], table_male.rows[0][3]);

	ASSERT_FALSE(table_male.is_default_value);

	ASSERT_EQ(col_size, table_feme.columns.size());
	ASSERT_GT(table_feme.rows.size(), 0);
	ASSERT_EQ(table_feme.rows[0][1], table_feme.rows[0][2]);
	ASSERT_NE(table_feme.rows[0][1], table_feme.rows[0][3]);
	ASSERT_FALSE(table_feme.is_default_value);
}

TEST(TestDatastore, RetrieveAnalysisEntity)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);
	auto uk = manager.get_country("GB");
	auto entity = manager.get_disease_analysis(uk.value());

	ASSERT_FALSE(entity.empty());
	ASSERT_GT(entity.disability_weights.size(), 0);
	ASSERT_GT(entity.cost_of_diseases.size(), 0);
	ASSERT_GT(entity.life_expectancy.size(), 0);
}

TEST(TestDatastore, RetrieveBirthIndicators)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);
	auto uk = manager.get_country("GB");
	auto uk_births = manager.get_birth_indicators(uk.value());

	// Filter out the ends of the years range
	auto uk_births_min = uk_births.front().time;
	auto uk_births_max = uk_births.back().time;
	auto mid_year = std::midpoint(uk_births_min, uk_births_max);
	auto uk_births_flt = manager.get_birth_indicators(uk.value(), [&mid_year](const int& value)
		{return value >= (mid_year - 1) && value <= (mid_year + 1); });

	ASSERT_FALSE(uk_births.empty());
	ASSERT_GT(uk_births.size(), 0);
	ASSERT_GT(uk_births_flt.size(), 0);
	ASSERT_GT(uk_births_max, uk_births_min);

	ASSERT_GT(uk_births.size(), uk_births_flt.size());
	ASSERT_LT(uk_births_min, uk_births_flt.front().time);
	ASSERT_GT(uk_births_max, uk_births_flt.back().time);
}

TEST(TestDatastore, RetrieveCancerDefinition)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto uk = manager.get_country("GB").value();
	auto diseases = manager.get_diseases();
	auto cancer_count = 0;
	for (auto& item : diseases) {
		if (item.group != DiseaseGroup::cancer) {
			continue;
		}

		cancer_count++;
		auto entity = manager.get_disease(item, uk);
		ASSERT_FALSE(entity.empty());
		ASSERT_EQ(entity.measures.size(), 4);
		ASSERT_GT(entity.items.size(), 0);
		EXPECT_EQ(item.code, entity.info.code);
		EXPECT_EQ(uk.code, entity.country.code);
	}

	ASSERT_GT(diseases.size(), 0);
	ASSERT_GT(cancer_count, 0);
}

TEST(TestDatastore, RetrieveCancerParameters)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto uk = manager.get_country("GB");
	auto diseases = manager.get_diseases();
	auto cancer_count = 0;
	for (auto& item : diseases) {
		if (item.group != DiseaseGroup::cancer) {
			continue;
		}

		cancer_count++;
		auto entity = manager.get_disease_parameter(item, uk.value());
		ASSERT_GT(entity.time_year, 0);
		ASSERT_FALSE(entity.prevalence_distribution.empty());
		ASSERT_FALSE(entity.survival_rate.empty());
		ASSERT_FALSE(entity.death_weight.empty());
	}

	ASSERT_GT(diseases.size(), 0);
	ASSERT_GT(cancer_count, 0);
}