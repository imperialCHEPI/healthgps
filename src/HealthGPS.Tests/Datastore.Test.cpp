#include "data_config.h"
#include "pch.h"

#include "HealthGPS.Datastore/api.h"

// The fixture for testing class Foo.
class DatastoreTest : public ::testing::Test {
  protected:
    DatastoreTest() : manager{test_datastore_path} {}

    hgps::data::DataManager manager;
    hgps::core::Country uk = manager.get_country("GB");
    hgps::core::Country india = manager.get_country("IN");
};

TEST_F(DatastoreTest, CreateDataManager) {
    auto countries = manager.get_countries();
    ASSERT_GT(countries.size(), 0);
}

TEST_F(DatastoreTest, CreateDataManagerFailWithWrongPath) {
    EXPECT_THROW(hgps::data::DataManager{"C:\\x\\y"}, std::runtime_error);
    EXPECT_THROW(hgps::data::DataManager{"C:/x/y"}, std::runtime_error);
    EXPECT_THROW(hgps::data::DataManager{"/home/x/y/z"}, std::runtime_error);
}

TEST_F(DatastoreTest, CountryMissingThrowsException) {
    ASSERT_THROW(manager.get_country("FAKE"), std::runtime_error);
}

TEST_F(DatastoreTest, CountryIsCaseInsensitive) {
    EXPECT_NO_THROW(manager.get_country("gb"));
    EXPECT_NO_THROW(manager.get_country("GB"));
    EXPECT_NO_THROW(manager.get_country("gbr"));
    EXPECT_NO_THROW(manager.get_country("GBR"));
}

TEST_F(DatastoreTest, CountryPopulation) {
    using namespace hgps::core;
    auto uk_pop = manager.get_population(uk);

    // Filter out the ends of the years range
    auto uk_pop_min = uk_pop.front().at_time;
    auto uk_pop_max = uk_pop.back().at_time;
    auto mid_year = std::midpoint(uk_pop_min, uk_pop_max);
    auto uk_pop_flt = manager.get_population(
        uk, [&mid_year](int value) { return value >= (mid_year - 1) && value <= (mid_year + 1); });

    ASSERT_GT(uk_pop.size(), 0);
    ASSERT_GT(uk_pop_flt.size(), 0);

    ASSERT_GT(uk_pop.size(), uk_pop_flt.size());
    ASSERT_LT(uk_pop_min, uk_pop_flt.front().at_time);
    ASSERT_GT(uk_pop_max, uk_pop_flt.back().at_time);

    auto table_pop = std::map<int, std::map<int, PopulationItem>>{};
    for (auto &item : uk_pop) {
        if (!table_pop.contains(item.at_time)) {
            table_pop.emplace(item.at_time, std::map<int, PopulationItem>{});
        }

        table_pop.at(item.at_time)
            .emplace(item.with_age, PopulationItem{.males = item.males, .females = item.females});
    }

    auto max_age = 100;
    for (auto year = uk_pop_min; year <= uk_pop_max; year++) {
        ASSERT_TRUE(table_pop.contains(year));

        auto &year_pop = table_pop.at(year);
        for (auto age = 0; age <= max_age; age++) {
            ASSERT_TRUE(year_pop.contains(age));
            auto &item = year_pop.at(age);
            ASSERT_GT(item.males, 0.0f);
            ASSERT_GT(item.females, 0.0f);
        }
    }
}

TEST_F(DatastoreTest, CountryMortality) {
    using namespace hgps::core;
    auto uk_deaths = manager.get_mortality(uk);

    // Filter out the ends of the years range
    auto uk_deaths_min = uk_deaths.front().at_time;
    auto uk_deaths_max = uk_deaths.back().at_time;
    auto mid_year = std::midpoint(uk_deaths_min, uk_deaths_max);
    auto uk_deaths_flt = manager.get_mortality(
        uk, [&mid_year](int value) { return value >= (mid_year - 1) && value <= (mid_year + 1); });

    ASSERT_GT(uk_deaths.size(), 0);
    ASSERT_GT(uk_deaths_flt.size(), 0);
    ASSERT_GT(uk_deaths_max, uk_deaths_min);

    ASSERT_GT(uk_deaths.size(), uk_deaths_flt.size());
    ASSERT_LT(uk_deaths_min, uk_deaths_flt.front().at_time);
    ASSERT_GT(uk_deaths_max, uk_deaths_flt.back().at_time);

    auto table_deaths = std::map<int, std::map<int, MortalityItem>>{};
    for (auto &item : uk_deaths) {
        if (!table_deaths.contains(item.at_time)) {
            table_deaths.emplace(item.at_time, std::map<int, MortalityItem>{});
        }

        table_deaths.at(item.at_time)
            .emplace(item.with_age, MortalityItem{.males = item.males, .females = item.females});
    }

    auto max_age = 100;
    for (auto year = uk_deaths_min; year <= uk_deaths_max; year++) {
        ASSERT_TRUE(table_deaths.contains(year));

        auto &year_deaths = table_deaths.at(year);
        for (auto age = 0; age <= max_age; age++) {
            ASSERT_TRUE(year_deaths.contains(age));
            auto &item = year_deaths.at(age);
            ASSERT_GT(item.males, 0.0f);
            ASSERT_GT(item.females, 0.0f);
        }
    }
}

TEST_F(DatastoreTest, GetDiseases) {
    auto diseases = manager.get_diseases();
    ASSERT_GT(diseases.size(), 0);
}

TEST_F(DatastoreTest, GetDiseaseInfoMatchesGetDisases) {
    auto diseases = manager.get_diseases();
    for (auto &item : diseases) {
        auto call = [&] {
            auto info = manager.get_disease_info(item.code);
            EXPECT_EQ(item.code, info.code);
        };
        EXPECT_NO_THROW(call());
    }
}

TEST_F(DatastoreTest, GetDiseaseInfoMissingThrowsException) {
    EXPECT_THROW(manager.get_disease_info("FAKE"), std::runtime_error);
}

TEST_F(DatastoreTest, RetrieveDiseasesTypeInInfo) {
    using namespace hgps::core;

    auto diseases = manager.get_diseases();
    auto cancer_count = 0;
    for (auto &item : diseases) {
        if (item.group == DiseaseGroup::cancer) {
            cancer_count++;
        }
    }

    ASSERT_GT(diseases.size(), 0);
    ASSERT_GT(cancer_count, 0);
}

TEST_F(DatastoreTest, RetrieveDiseaseDefinition) {
    auto diseases = manager.get_diseases();
    for (auto &item : diseases) {
        auto entity = manager.get_disease(item, india);

        ASSERT_FALSE(entity.empty());
        ASSERT_GT(entity.measures.size(), 0);
        ASSERT_GT(entity.items.size(), 0);
        EXPECT_EQ(item.code, entity.info.code);
        EXPECT_EQ(india.code, entity.country.code);
    }
}

TEST_F(DatastoreTest, RetrieveDiseaseDefinitionIsEmpty) {
    using namespace hgps::core;

    auto info = DiseaseInfo{.group = DiseaseGroup::other,
                            .code = Identifier{"ghost369"},
                            .name = "Look at the flowers."};

    EXPECT_THROW(manager.get_disease(info, india), std::runtime_error);
}

TEST_F(DatastoreTest, DiseaseRelativeRiskToDisease) {
    auto asthma = manager.get_disease_info("asthma");
    auto diabetes = manager.get_disease_info("diabetes");

    auto table_self = manager.get_relative_risk_to_disease(diabetes, diabetes);
    auto table_other = manager.get_relative_risk_to_disease(diabetes, asthma);

    // diabetes to diabetes files-based default values
    EXPECT_FALSE(table_self.has_value());

    ASSERT_TRUE(table_other.has_value());
    EXPECT_EQ(3, table_other->columns.size());
    EXPECT_GT(table_other->rows.size(), 0);
    EXPECT_EQ(table_other->rows[0][1], table_other->rows[0][2]);
}

TEST_F(DatastoreTest, MissingDiseaseRelativeRiskToDisease) {
    using namespace hgps::core;

    auto diabetes = manager.get_disease_info("diabetes");
    auto info = DiseaseInfo{.group = DiseaseGroup::other,
                            .code = Identifier{"ghost369"},
                            .name = "Look at the flowers."};

    ASSERT_FALSE(manager.get_relative_risk_to_disease(diabetes, info).has_value());
}

TEST_F(DatastoreTest, DiseaseRelativeRiskToRiskFactor) {
    using namespace hgps::core;

    auto risk_factor = Identifier{"bmi"};
    auto diabetes = manager.get_disease_info("diabetes");

    auto col_size = 8;

    auto table_male = manager.get_relative_risk_to_risk_factor(diabetes, Gender::male, risk_factor);
    ASSERT_TRUE(table_male.has_value());
    EXPECT_EQ(col_size, table_male->columns.size());
    EXPECT_FALSE(table_male->rows.empty());
    EXPECT_EQ(table_male->rows[0][1], table_male->rows[0][2]);
    EXPECT_NE(table_male->rows[0][1], table_male->rows[0][3]);

    auto table_female =
        manager.get_relative_risk_to_risk_factor(diabetes, Gender::female, risk_factor);
    ASSERT_TRUE(table_female.has_value());
    EXPECT_EQ(col_size, table_female->columns.size());
    EXPECT_FALSE(table_female->rows.empty());
    EXPECT_EQ(table_female->rows[0][1], table_female->rows[0][2]);
    EXPECT_NE(table_female->rows[0][1], table_female->rows[0][3]);
}

TEST_F(DatastoreTest, RetrieveAnalysisEntity) {
    auto entity = manager.get_disease_analysis(uk);

    ASSERT_FALSE(entity.empty());
    ASSERT_GT(entity.disability_weights.size(), 0);
    ASSERT_GT(entity.cost_of_diseases.size(), 0);
    ASSERT_GT(entity.life_expectancy.size(), 0);
}

TEST_F(DatastoreTest, RetrieveBirthIndicators) {
    auto uk_births = manager.get_birth_indicators(uk);

    // Filter out the ends of the years range
    auto uk_births_min = uk_births.front().at_time;
    auto uk_births_max = uk_births.back().at_time;
    auto mid_year = std::midpoint(uk_births_min, uk_births_max);
    auto uk_births_flt = manager.get_birth_indicators(
        uk, [&mid_year](int value) { return value >= (mid_year - 1) && value <= (mid_year + 1); });

    ASSERT_FALSE(uk_births.empty());
    ASSERT_GT(uk_births.size(), 0);
    ASSERT_GT(uk_births_flt.size(), 0);
    ASSERT_GT(uk_births_max, uk_births_min);

    ASSERT_GT(uk_births.size(), uk_births_flt.size());
    ASSERT_LT(uk_births_min, uk_births_flt.front().at_time);
    ASSERT_GT(uk_births_max, uk_births_flt.back().at_time);
}

TEST_F(DatastoreTest, RetrieveCancerDefinition) {
    using namespace hgps::core;

    auto diseases = manager.get_diseases();
    auto cancer_count = 0;
    for (auto &item : diseases) {
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

TEST_F(DatastoreTest, RetrieveCancerParameters) {
    using namespace hgps::core;

    auto diseases = manager.get_diseases();
    auto cancer_count = 0;
    for (auto &item : diseases) {
        if (item.group != DiseaseGroup::cancer) {
            continue;
        }

        cancer_count++;
        auto entity = manager.get_disease_parameter(item, uk);
        ASSERT_GT(entity.at_time, 0);
        ASSERT_FALSE(entity.prevalence_distribution.empty());
        ASSERT_FALSE(entity.survival_rate.empty());
        ASSERT_FALSE(entity.death_weight.empty());
    }

    ASSERT_GT(diseases.size(), 0u);
    ASSERT_GT(cancer_count, 0);
}

TEST_F(DatastoreTest, RetrieveLmsParameters) {
    using namespace hgps::core;

    auto lms = manager.get_lms_parameters();
    auto male_count =
        std::count_if(lms.begin(), lms.end(), [](auto &row) { return row.gender == Gender::male; });

    auto feme_count = std::count_if(lms.begin(), lms.end(),
                                    [](auto &row) { return row.gender == Gender::female; });

    ASSERT_FALSE(lms.empty());
    ASSERT_GT(male_count, 0u);
    ASSERT_LT(static_cast<size_t>(male_count), lms.size());
    ASSERT_EQ(male_count, feme_count);
}
