#include "pch.h"

#include "HealthGPS.Core/identifier.h"
#include "HealthGPS.Core/interval.h"
#include "HealthGPS/demographic.h"
#include "HealthGPS/disease_module.h"
#include "HealthGPS/gender_table.h"
#include "HealthGPS/gender_value.h"
#include "HealthGPS/life_table.h"
#include "HealthGPS/person.h"
#include "HealthGPS/population_record.h"
#include "HealthGPS/runtime_context.h"

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <vector>

using namespace hgps;
using namespace hgps::core;

class DemographicModuleTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create test population data
        create_test_population_data();
        create_test_life_table();
    }

    void create_test_population_data() {
        // Create population data for years 2020-2025, ages 0-100
        for (int year = 2020; year <= 2025; year++) {
            std::map<int, PopulationRecord> year_data;
            for (int age = 0; age <= 100; age++) {
                // Create realistic population distribution
                double males = 1000.0 * exp(-age * 0.02) * (1.0 + 0.01 * (year - 2020));
                double females = 950.0 * exp(-age * 0.02) * (1.0 + 0.01 * (year - 2020));

                year_data[age] =
                    PopulationRecord(age, static_cast<float>(males), static_cast<float>(females));
            }
            pop_data_[year] = year_data;
        }
    }

    void create_test_life_table() {
        // Create birth indicators
        std::vector<BirthIndicator> births;
        for (int year = 2020; year <= 2025; year++) {
            births.push_back(
                BirthIndicator(year, 1000.0 + year * 10, 1.05)); // More boys than girls
        }

        // Create mortality data
        std::vector<MortalityIndicator> deaths;
        for (int year = 2020; year <= 2025; year++) {
            for (int age = 0; age <= 100; age++) {
                // Create realistic mortality rates
                double base_rate = 0.001 + age * 0.0001;
                double male_rate = base_rate * 1.1;
                double female_rate = base_rate * 0.9;

                deaths.push_back(MortalityIndicator(year, age, static_cast<float>(male_rate),
                                                    static_cast<float>(female_rate)));
            }
        }

        life_table_ = LifeTable(births, deaths);
    }

    // Test data members
    std::map<int, std::map<int, PopulationRecord>> pop_data_;
    LifeTable life_table_;
};

TEST_F(DemographicModuleTest, Constructor) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    EXPECT_EQ(SimulationModuleType::Demographic, module->type());
    EXPECT_EQ("Demographic", module->name());
}

TEST_F(DemographicModuleTest, ConstructorWithEmptyPopulationData) {
    std::map<int, std::map<int, PopulationRecord>> empty_pop_data;
    LifeTable empty_life_table;

    auto module =
        std::make_unique<DemographicModule>(std::move(empty_pop_data), std::move(empty_life_table));

    EXPECT_EQ(SimulationModuleType::Demographic, module->type());
    EXPECT_EQ("Demographic", module->name());
}

TEST_F(DemographicModuleTest, ConstructorWithMismatchedData) {
    std::map<int, std::map<int, PopulationRecord>> empty_pop_data;

    // Create non-empty life table
    std::vector<BirthIndicator> births;
    births.push_back(BirthIndicator(2020, 1000.0, 1.05));
    std::vector<MortalityIndicator> deaths;
    deaths.push_back(MortalityIndicator(2020, 0, 0.001f, 0.001f));
    LifeTable non_empty_life_table(births, deaths);

    EXPECT_THROW(DemographicModule(std::move(empty_pop_data), std::move(non_empty_life_table)),
                 std::invalid_argument);
}

TEST_F(DemographicModuleTest, ConstructorWithEmptyLifeTable) {
    std::map<int, std::map<int, PopulationRecord>> non_empty_pop_data;
    non_empty_pop_data[2020][0] = PopulationRecord(0, 1000.0f, 950.0f);

    LifeTable empty_life_table;

    EXPECT_THROW(DemographicModule(std::move(non_empty_pop_data), std::move(empty_life_table)),
                 std::invalid_argument);
}

TEST_F(DemographicModuleTest, ConstructorWithTimeRangeMismatch) {
    // Create population data for 2020-2022
    std::map<int, std::map<int, PopulationRecord>> pop_data;
    for (int year = 2020; year <= 2022; year++) {
        std::map<int, PopulationRecord> year_data;
        year_data[0] = PopulationRecord(0, 1000.0f, 950.0f);
        pop_data[year] = year_data;
    }

    // Create life table for 2020-2025
    std::vector<BirthIndicator> births;
    for (int year = 2020; year <= 2025; year++) {
        births.push_back(BirthIndicator(year, 1000.0, 1.05));
    }
    std::vector<MortalityIndicator> deaths;
    for (int year = 2020; year <= 2025; year++) {
        deaths.push_back(MortalityIndicator(year, 0, 0.001f, 0.001f));
    }
    LifeTable life_table(births, deaths);

    EXPECT_THROW(DemographicModule(std::move(pop_data), std::move(life_table)),
                 std::invalid_argument);
}

TEST_F(DemographicModuleTest, ConstructorWithAgeRangeMismatch) {
    // Create population data with ages 0-50
    std::map<int, std::map<int, PopulationRecord>> pop_data;
    for (int year = 2020; year <= 2022; year++) {
        std::map<int, PopulationRecord> year_data;
        for (int age = 0; age <= 50; age++) {
            year_data[age] = PopulationRecord(age, 1000.0f, 950.0f);
        }
        pop_data[year] = year_data;
    }

    // Create life table with ages 0-100
    std::vector<BirthIndicator> births;
    births.push_back(BirthIndicator(2020, 1000.0, 1.05));
    std::vector<MortalityIndicator> deaths;
    for (int year = 2020; year <= 2022; year++) {
        for (int age = 0; age <= 100; age++) {
            deaths.push_back(MortalityIndicator(year, age, 0.001f, 0.001f));
        }
    }
    LifeTable life_table(births, deaths);

    EXPECT_THROW(DemographicModule(std::move(pop_data), std::move(life_table)),
                 std::invalid_argument);
}

TEST_F(DemographicModuleTest, GetTotalPopulationSize) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Test with valid year
    std::size_t size_2020 = module->get_total_population_size(2020);
    EXPECT_GT(size_2020, 0);

    // Test with invalid year
    std::size_t size_invalid = module->get_total_population_size(1999);
    EXPECT_EQ(0, size_invalid);
}

TEST_F(DemographicModuleTest, GetTotalDeaths) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Test with valid year
    double deaths_2020 = module->get_total_deaths(2020);
    EXPECT_GE(deaths_2020, 0.0);

    // Test with invalid year
    double deaths_invalid = module->get_total_deaths(1999);
    EXPECT_EQ(0.0, deaths_invalid);
}

TEST_F(DemographicModuleTest, GetPopulationDistribution) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Test with valid year
    const auto &dist_2020 = module->get_population_distribution(2020);
    EXPECT_FALSE(dist_2020.empty());
    EXPECT_TRUE(dist_2020.contains(0));
    EXPECT_TRUE(dist_2020.contains(50));
    EXPECT_TRUE(dist_2020.contains(100));

    // Test with invalid year - should throw
    EXPECT_THROW(module->get_population_distribution(1999), std::out_of_range);
}

TEST_F(DemographicModuleTest, GetAgeGenderDistribution) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Test with valid year
    auto dist_2020 = module->get_age_gender_distribution(2020);
    EXPECT_FALSE(dist_2020.empty());

    // Check that ratios sum to approximately 1.0
    double total_ratio = 0.0;
    for (const auto &[age, gender_value] : dist_2020) {
        total_ratio += gender_value.males + gender_value.females;
    }
    EXPECT_NEAR(1.0, total_ratio, 0.01);

    // Test with invalid year
    auto dist_invalid = module->get_age_gender_distribution(1999);
    EXPECT_TRUE(dist_invalid.empty());
}

TEST_F(DemographicModuleTest, GetBirthRate) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Test with valid year
    auto birth_rate_2020 = module->get_birth_rate(2020);
    EXPECT_GE(birth_rate_2020.males, 0.0);
    EXPECT_GE(birth_rate_2020.females, 0.0);

    // Test with invalid year
    auto birth_rate_invalid = module->get_birth_rate(1999);
    EXPECT_EQ(0.0, birth_rate_invalid.males);
    EXPECT_EQ(0.0, birth_rate_invalid.females);
}

TEST_F(DemographicModuleTest, GetResidualDeathRate) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Test with valid age and gender
    double death_rate_male = module->get_residual_death_rate(30, Gender::male);
    EXPECT_GE(death_rate_male, 0.0);
    EXPECT_LE(death_rate_male, 1.0);

    double death_rate_female = module->get_residual_death_rate(30, Gender::female);
    EXPECT_GE(death_rate_female, 0.0);
    EXPECT_LE(death_rate_female, 1.0);

    // Test with invalid age
    double death_rate_invalid = module->get_residual_death_rate(150, Gender::male);
    EXPECT_EQ(0.0, death_rate_invalid);
}

TEST_F(DemographicModuleTest, CreateDeathRatesTable) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Test creating death rates table
    auto death_rates = module->create_death_rates_table(2020);

    // Should have entries for all ages
    EXPECT_FALSE(death_rates.empty());

    // Check that death rates are reasonable (0 <= rate <= 1)
    for (const auto &[age, gender_rates] : death_rates) {
        EXPECT_GE(gender_rates.at(Gender::male), 0.0);
        EXPECT_LE(gender_rates.at(Gender::male), 1.0);
        EXPECT_GE(gender_rates.at(Gender::female), 0.0);
        EXPECT_LE(gender_rates.at(Gender::female), 1.0);
    }
}

TEST_F(DemographicModuleTest, CalculateResidualMortality) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Create a mock disease module
    auto mock_disease_module = std::make_shared<DiseaseModule>();

    // Test calculating residual mortality
    auto residual_mortality =
        module->calculate_residual_mortality(*mock_context, *mock_disease_module);

    // Should have entries for all ages
    EXPECT_FALSE(residual_mortality.empty());

    // Check that residual mortality rates are reasonable (0 <= rate <= 1)
    for (const auto &[age, gender_rates] : residual_mortality) {
        EXPECT_GE(gender_rates.at(Gender::male), 0.0);
        EXPECT_LE(gender_rates.at(Gender::male), 1.0);
        EXPECT_GE(gender_rates.at(Gender::female), 0.0);
        EXPECT_LE(gender_rates.at(Gender::female), 1.0);
    }
}

TEST_F(DemographicModuleTest, CalculateExcessMortalityProduct) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Create a mock person
    Person person;
    person.age = 30;
    person.gender = Gender::male;

    // Create a mock disease module
    auto mock_disease_module = std::make_shared<DiseaseModule>();

    // Test calculating excess mortality product
    double product = module->calculate_excess_mortality_product(person, *mock_disease_module);

    // Should be between 0 and 1
    EXPECT_GE(product, 0.0);
    EXPECT_LE(product, 1.0);
}

TEST_F(DemographicModuleTest, UpdateAgeAndDeathEvents) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Create a mock disease module
    auto mock_disease_module = std::make_shared<DiseaseModule>();

    // Test updating age and death events
    int deaths = module->update_age_and_death_events(*mock_context, *mock_disease_module);

    // Should return a non-negative number of deaths
    EXPECT_GE(deaths, 0);
}

TEST_F(DemographicModuleTest, InitialisePopulation) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Test initialising population
    module->initialise_population(*mock_context);

    // Should not throw any exceptions
    SUCCEED();
}

TEST_F(DemographicModuleTest, UpdatePopulation) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Create a mock disease module
    auto mock_disease_module = std::make_shared<DiseaseModule>();

    // Test updating population
    module->update_population(*mock_context, *mock_disease_module);

    // Should not throw any exceptions
    SUCCEED();
}

TEST_F(DemographicModuleTest, UpdateResidualMortality) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Create a mock runtime context
    auto mock_context = std::make_shared<RuntimeContext>(nullptr, nullptr, nullptr);

    // Create a mock disease module
    auto mock_disease_module = std::make_shared<DiseaseModule>();

    // Test updating residual mortality
    module->update_residual_mortality(*mock_context, *mock_disease_module);

    // Should not throw any exceptions
    SUCCEED();
}

TEST_F(DemographicModuleTest, InitialiseBirthRates) {
    auto module = std::make_unique<DemographicModule>(std::move(pop_data_), std::move(life_table_));

    // Test that birth rates are initialised
    auto birth_rate_2020 = module->get_birth_rate(2020);
    EXPECT_GE(birth_rate_2020.males, 0.0);
    EXPECT_GE(birth_rate_2020.females, 0.0);

    // Birth rates should be consistent with life table data
    auto birth_rate_2021 = module->get_birth_rate(2021);
    EXPECT_GE(birth_rate_2021.males, 0.0);
    EXPECT_GE(birth_rate_2021.females, 0.0);
}

TEST_F(DemographicModuleTest, PopulationRecordStructure) {
    PopulationRecord record(25, 1000.0f, 950.0f);

    EXPECT_EQ(25, record.age);
    EXPECT_EQ(1000.0f, record.males);
    EXPECT_EQ(950.0f, record.females);
    EXPECT_EQ(1950.0f, record.total());
}

TEST_F(DemographicModuleTest, PopulationRecordTotal) {
    PopulationRecord record(30, 1200.0f, 1100.0f);

    EXPECT_EQ(2300.0f, record.total());
}

TEST_F(DemographicModuleTest, PopulationRecordZeroValues) {
    PopulationRecord record(0, 0.0f, 0.0f);

    EXPECT_EQ(0, record.age);
    EXPECT_EQ(0.0f, record.males);
    EXPECT_EQ(0.0f, record.females);
    EXPECT_EQ(0.0f, record.total());
}

TEST_F(DemographicModuleTest, PopulationRecordNegativeValues) {
    PopulationRecord record(-1, -100.0f, -50.0f);

    EXPECT_EQ(-1, record.age);
    EXPECT_EQ(-100.0f, record.males);
    EXPECT_EQ(-50.0f, record.females);
    EXPECT_EQ(-150.0f, record.total());
}

TEST_F(DemographicModuleTest, PopulationRecordLargeValues) {
    PopulationRecord record(100, 1000000.0f, 950000.0f);

    EXPECT_EQ(100, record.age);
    EXPECT_EQ(1000000.0f, record.males);
    EXPECT_EQ(950000.0f, record.females);
    EXPECT_EQ(1950000.0f, record.total());
}
