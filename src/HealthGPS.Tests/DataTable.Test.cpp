#include "HealthGPS.Core/datatable.h"
#include "HealthGPS.Core/api.h"
#include "pch.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Tests for datatable.cpp
// Created- Mahima
// Comprehensive tests for demographic coefficients processing

using namespace hgps::core;

// Helper function to create a test DataTable
DataTable create_test_table() {
    DataTable table;
    table.add(std::make_unique<IntegerDataTableColumn>("id", std::vector<int>{1, 2, 3}));
    table.add(
        std::make_unique<StringDataTableColumn>("name", std::vector<std::string>{"A", "B", "C"}));
    return table;
}

// Test the demographic coefficient getters and setters - lines 124, 129
TEST(DataTableTest, DemographicCoefficientsBasicOperations) {
    DataTable table = create_test_table();

    // Create test demographic coefficients
    DataTable::DemographicCoefficients coeffs;
    coeffs.age_coefficient = 0.5;
    coeffs.gender_coefficients[Gender::male] = 1.0;
    coeffs.gender_coefficients[Gender::female] = 1.2;
    coeffs.region_coefficients[Region::England] = 2.0;
    coeffs.ethnicity_coefficients[Ethnicity::White] = 3.0;

    // Set the coefficients
    table.set_demographic_coefficients("test_model", coeffs);

    // Get the coefficients and verify they match
    auto retrieved_coeffs = table.get_demographic_coefficients("test_model");

    ASSERT_DOUBLE_EQ(0.5, retrieved_coeffs.age_coefficient);
    ASSERT_DOUBLE_EQ(1.0, retrieved_coeffs.gender_coefficients[Gender::male]);
    ASSERT_DOUBLE_EQ(1.2, retrieved_coeffs.gender_coefficients[Gender::female]);
    ASSERT_DOUBLE_EQ(2.0, retrieved_coeffs.region_coefficients[Region::England]);
    ASSERT_DOUBLE_EQ(3.0, retrieved_coeffs.ethnicity_coefficients[Ethnicity::White]);

    // Test getting non-existent model
    ASSERT_THROW(table.get_demographic_coefficients("nonexistent"), std::runtime_error);
}

// Test calculating probability - lines 133-134, 136-137
TEST(DataTableTest, CalculateProbability) {
    // Create test coefficients
    DataTable::DemographicCoefficients coeffs;
    coeffs.age_coefficient = 0.05; // 0.05 * age
    coeffs.gender_coefficients[Gender::male] = 0.1;
    coeffs.gender_coefficients[Gender::female] = 0.2;
    coeffs.region_coefficients[Region::England] = 0.3;
    coeffs.region_coefficients[Region::Wales] = 0.4;
    coeffs.ethnicity_coefficients[Ethnicity::White] = 0.5;
    coeffs.ethnicity_coefficients[Ethnicity::Asian] = 0.6;

    // Test with male, England, White
    double prob1 = DataTable::calculate_probability(coeffs, 2, Gender::male, Region::England,
                                                    Ethnicity::White);
    // Expected: (1 + 0.05*2) * (1 + 0.1) * (1 + 0.3) * (1 + 0.5) = 1.1 * 1.1 * 1.3 * 1.5 = 2.34
    // But it's clamped to 1.0
    ASSERT_DOUBLE_EQ(1.0, prob1);

    // Test with very small values to stay under 1.0
    DataTable::DemographicCoefficients small_coeffs;
    small_coeffs.age_coefficient = 0.01;
    small_coeffs.gender_coefficients[Gender::male] = 0.01;
    small_coeffs.gender_coefficients[Gender::female] = 0.01;
    small_coeffs.region_coefficients[Region::England] = 0.01;
    small_coeffs.region_coefficients[Region::Wales] = 0.01;
    small_coeffs.ethnicity_coefficients[Ethnicity::White] = 0.01;
    small_coeffs.ethnicity_coefficients[Ethnicity::Asian] = 0.01;

    // Test with male, England, White with small coefficients
    double prob2 = DataTable::calculate_probability(small_coeffs, 1, Gender::male, Region::England,
                                                    Ethnicity::White);
    // Expected: (1 + 0.01*1) * (1 + 0.01) * (1 + 0.01) * (1 + 0.01) ≈ 1.04...
    ASSERT_NEAR(1.04, prob2, 0.01);

    // Test without ethnicity
    double prob3 = DataTable::calculate_probability(small_coeffs, 1, Gender::male, Region::England);
    // Expected: (1 + 0.01*1) * (1 + 0.01) * (1 + 0.01) ≈ 1.03...
    ASSERT_NEAR(1.03, prob3, 0.01);
}

// Test loading demographic coefficients from JSON - specifically region_coefficients - lines
// 142-149
TEST(DataTableTest, LoadRegionCoefficients) {
    DataTable table = create_test_table();

    // Create a test JSON with region coefficients
    nlohmann::json test_json = {{"modelling",
                                 {{"demographic_models",
                                   {{"region",
                                     {{"probabilities",
                                       {{"coefficients",
                                         {{"age", 0.1},
                                          {"gender", {{"male", 0.2}, {"female", 0.3}}},
                                          {"region",
                                           {{"England", 0.4},
                                            {"Wales", 0.5},
                                            {"Scotland", 0.6},
                                            {"NorthernIreland", 0.7}}}}}}}}}}}}}};

    // Load the coefficients
    table.load_demographic_coefficients(test_json);

    // Get and verify the region coefficients
    auto coeffs = table.get_demographic_coefficients("region.probabilities");

    ASSERT_DOUBLE_EQ(0.1, coeffs.age_coefficient);
    ASSERT_DOUBLE_EQ(0.2, coeffs.gender_coefficients[Gender::male]);
    ASSERT_DOUBLE_EQ(0.3, coeffs.gender_coefficients[Gender::female]);
    ASSERT_DOUBLE_EQ(0.4, coeffs.region_coefficients[Region::England]);
    ASSERT_DOUBLE_EQ(0.5, coeffs.region_coefficients[Region::Wales]);
    ASSERT_DOUBLE_EQ(0.6, coeffs.region_coefficients[Region::Scotland]);
    ASSERT_DOUBLE_EQ(0.7, coeffs.region_coefficients[Region::NorthernIreland]);
}

// Test loading ethnicity coefficients - lines 153, 158, 163, 167-168, 172, 174, 178, 182
TEST(DataTableTest, LoadEthnicityCoefficients) {
    DataTable table = create_test_table();

    // Create a test JSON with ethnicity coefficients
    nlohmann::json test_json = {
        {"modelling",
         {{"demographic_models",
           {{"ethnicity",
             {{"probabilities",
               {{"coefficients",
                 {{"age", 0.1},
                  {"gender", {{"male", 0.2}, {"female", 0.3}}},
                  {"region",
                   {{"England", 0.4}, {"Wales", 0.5}, {"Scotland", 0.6}, {"NorthernIreland", 0.7}}},
                  {"ethnicity",
                   {{"White", 0.8}, {"Asian", 0.9}, {"Black", 1.0}, {"Others", 1.1}}}}}}}}}}}}}};

    // Load the coefficients
    table.load_demographic_coefficients(test_json);

    // Get and verify the ethnicity coefficients
    auto coeffs = table.get_demographic_coefficients("ethnicity.probabilities");

    ASSERT_DOUBLE_EQ(0.1, coeffs.age_coefficient);
    ASSERT_DOUBLE_EQ(0.2, coeffs.gender_coefficients[Gender::male]);
    ASSERT_DOUBLE_EQ(0.3, coeffs.gender_coefficients[Gender::female]);
    ASSERT_DOUBLE_EQ(0.4, coeffs.region_coefficients[Region::England]);
    ASSERT_DOUBLE_EQ(0.5, coeffs.region_coefficients[Region::Wales]);
    ASSERT_DOUBLE_EQ(0.6, coeffs.region_coefficients[Region::Scotland]);
    ASSERT_DOUBLE_EQ(0.7, coeffs.region_coefficients[Region::NorthernIreland]);
    ASSERT_DOUBLE_EQ(0.8, coeffs.ethnicity_coefficients[Ethnicity::White]);
    ASSERT_DOUBLE_EQ(0.9, coeffs.ethnicity_coefficients[Ethnicity::Asian]);
    ASSERT_DOUBLE_EQ(1.0, coeffs.ethnicity_coefficients[Ethnicity::Black]);
    ASSERT_DOUBLE_EQ(1.1, coeffs.ethnicity_coefficients[Ethnicity::Others]);
}

// Test edge cases for ethnicity coefficient loading - missing segments
TEST(DataTableTest, LoadEthnicityCoefficientsMissingData) {
    DataTable table = create_test_table();

    // Test with missing ethnicity section
    nlohmann::json test_json1 = {{"demographic_models", {}}};
    ASSERT_NO_THROW(table.load_demographic_coefficients(test_json1));

    // Test with missing probabilities section
    nlohmann::json test_json2 = {{"demographic_models", {{"ethnicity", {}}}}};
    ASSERT_NO_THROW(table.load_demographic_coefficients(test_json2));

    // Test with missing coefficients section
    nlohmann::json test_json3 = {{"demographic_models", {{"ethnicity", {{"probabilities", {}}}}}}};
    ASSERT_NO_THROW(table.load_demographic_coefficients(test_json3));

    // Test with empty coefficients section
    nlohmann::json test_json4 = {
        {"demographic_models", {{"ethnicity", {{"probabilities", {{"coefficients", {}}}}}}}}};
    ASSERT_NO_THROW(table.load_demographic_coefficients(test_json4));
}

// Test region distribution calculation - lines 184, 186-187, 189, 190
TEST(DataTableTest, GetRegionDistribution) {
    // Create a table with region and region_prob columns
    DataTable table;
    table.add(std::make_unique<StringDataTableColumn>(
        "region", std::vector<std::string>{"England", "Wales", "Scotland", "NorthernIreland"}));
    table.add(std::make_unique<DoubleDataTableColumn>("region_prob",
                                                      std::vector<double>{0.5, 0.2, 0.2, 0.1}));

    // Set up region coefficients
    DataTable::DemographicCoefficients coeffs;
    coeffs.age_coefficient = 0.05; // 0.05 * age
    coeffs.gender_coefficients[Gender::male] = 0.1;
    coeffs.gender_coefficients[Gender::female] = 0.2;
    coeffs.region_coefficients[Region::England] = 0.3;
    coeffs.region_coefficients[Region::Wales] = 0.4;
    coeffs.region_coefficients[Region::Scotland] = 0.5;
    coeffs.region_coefficients[Region::NorthernIreland] = 0.6;
    table.set_demographic_coefficients("region.probabilities", coeffs);

    // Get region distribution for male, age 30
    auto distribution = table.get_region_distribution(30, Gender::male);

    // Verify all regions are present
    ASSERT_EQ(4, distribution.size());
    ASSERT_TRUE(distribution.contains(Region::England));
    ASSERT_TRUE(distribution.contains(Region::Wales));
    ASSERT_TRUE(distribution.contains(Region::Scotland));
    ASSERT_TRUE(distribution.contains(Region::NorthernIreland));

    // Calculate expected probabilities
    double eng_prob = 0.5 * (1.0 + 0.05 * 30 + 0.1 + 0.3);
    double wal_prob = 0.2 * (1.0 + 0.05 * 30 + 0.1 + 0.4);
    double sco_prob = 0.2 * (1.0 + 0.05 * 30 + 0.1 + 0.5);
    double ni_prob = 0.1 * (1.0 + 0.05 * 30 + 0.1 + 0.6);
    double total = eng_prob + wal_prob + sco_prob + ni_prob;

    // Verify distribution is properly normalized
    ASSERT_NEAR(eng_prob / total, distribution[Region::England], 0.01);
    ASSERT_NEAR(wal_prob / total, distribution[Region::Wales], 0.01);
    ASSERT_NEAR(sco_prob / total, distribution[Region::Scotland], 0.01);
    ASSERT_NEAR(ni_prob / total, distribution[Region::NorthernIreland], 0.01);

    // Verify the sum is very close to 1.0
    double sum = distribution[Region::England] + distribution[Region::Wales] +
                 distribution[Region::Scotland] + distribution[Region::NorthernIreland];
    ASSERT_NEAR(1.0, sum, 1e-10);
}

// Test ethnicity distribution calculation - lines 192, 197, 201-202, 204-205
TEST(DataTableTest, GetEthnicityDistribution) {
    // Create a table with ethnicity and ethnicity_prob columns
    DataTable table;
    table.add(std::make_unique<StringDataTableColumn>(
        "ethnicity", std::vector<std::string>{"White", "Asian", "Black", "Others"}));
    table.add(std::make_unique<DoubleDataTableColumn>("ethnicity_prob",
                                                      std::vector<double>{0.7, 0.1, 0.1, 0.1}));

    // Set up ethnicity coefficients
    DataTable::DemographicCoefficients coeffs;
    coeffs.age_coefficient = 0.05; // 0.05 * age
    coeffs.gender_coefficients[Gender::male] = 0.1;
    coeffs.gender_coefficients[Gender::female] = 0.2;
    coeffs.region_coefficients[Region::England] = 0.3;
    coeffs.region_coefficients[Region::Wales] = 0.4;
    coeffs.ethnicity_coefficients[Ethnicity::White] = 0.5;
    coeffs.ethnicity_coefficients[Ethnicity::Asian] = 0.6;
    coeffs.ethnicity_coefficients[Ethnicity::Black] = 0.7;
    coeffs.ethnicity_coefficients[Ethnicity::Others] = 0.8;
    table.set_demographic_coefficients("ethnicity.probabilities", coeffs);

    // Get ethnicity distribution for male, age 30, England
    auto distribution = table.get_ethnicity_distribution(30, Gender::male, Region::England);

    // Verify all ethnicities are present
    ASSERT_EQ(4, distribution.size());
    ASSERT_TRUE(distribution.contains(Ethnicity::White));
    ASSERT_TRUE(distribution.contains(Ethnicity::Asian));
    ASSERT_TRUE(distribution.contains(Ethnicity::Black));
    ASSERT_TRUE(distribution.contains(Ethnicity::Others));

    // Calculate expected probabilities
    double white_prob = 0.7 * (1.0 + 0.05 * 30 + 0.1 + 0.3 + 0.5);
    double asian_prob = 0.1 * (1.0 + 0.05 * 30 + 0.1 + 0.3 + 0.6);
    double black_prob = 0.1 * (1.0 + 0.05 * 30 + 0.1 + 0.3 + 0.7);
    double others_prob = 0.1 * (1.0 + 0.05 * 30 + 0.1 + 0.3 + 0.8);
    double total = white_prob + asian_prob + black_prob + others_prob;

    // Verify distribution is properly normalized
    ASSERT_NEAR(white_prob / total, distribution[Ethnicity::White], 0.01);
    ASSERT_NEAR(asian_prob / total, distribution[Ethnicity::Asian], 0.01);
    ASSERT_NEAR(black_prob / total, distribution[Ethnicity::Black], 0.01);
    ASSERT_NEAR(others_prob / total, distribution[Ethnicity::Others], 0.01);

    // Verify the sum is very close to 1.0
    double sum = distribution[Ethnicity::White] + distribution[Ethnicity::Asian] +
                 distribution[Ethnicity::Black] + distribution[Ethnicity::Others];
    ASSERT_NEAR(1.0, sum, 1e-10);

    // Test with Wales region to ensure different region coefficient works
    auto distribution2 = table.get_ethnicity_distribution(30, Gender::male, Region::Wales);

    // Calculate probabilities with Wales
    white_prob = 0.7 * (1.0 + 0.05 * 30 + 0.1 + 0.4 + 0.5);
    asian_prob = 0.1 * (1.0 + 0.05 * 30 + 0.1 + 0.4 + 0.6);
    black_prob = 0.1 * (1.0 + 0.05 * 30 + 0.1 + 0.4 + 0.7);
    others_prob = 0.1 * (1.0 + 0.05 * 30 + 0.1 + 0.4 + 0.8);
    total = white_prob + asian_prob + black_prob + others_prob;

    // Verify Wales distribution is properly normalized
    ASSERT_NEAR(white_prob / total, distribution2[Ethnicity::White], 0.01);

    // Test with female to ensure different gender coefficient works
    auto distribution3 = table.get_ethnicity_distribution(30, Gender::female, Region::England);

    // Calculate probabilities with female
    white_prob = 0.7 * (1.0 + 0.05 * 30 + 0.2 + 0.3 + 0.5);
    asian_prob = 0.1 * (1.0 + 0.05 * 30 + 0.2 + 0.3 + 0.6);
    black_prob = 0.1 * (1.0 + 0.05 * 30 + 0.2 + 0.3 + 0.7);
    others_prob = 0.1 * (1.0 + 0.05 * 30 + 0.2 + 0.3 + 0.8);
    total = white_prob + asian_prob + black_prob + others_prob;

    // Verify female distribution is properly normalized
    ASSERT_NEAR(white_prob / total, distribution3[Ethnicity::White], 0.01);
}

// Test boundary conditions for calculate_probability - lines 272-287
// Created - Mahima
TEST(DataTableTest, CalculateProbabilityBoundaries) {
    // Test age coefficient clamping
    {
        DataTable::DemographicCoefficients coeffs;
        coeffs.age_coefficient = 0.05; // With age 20, this gives 1.0 effect when multiplied

        // Test with high values (clamped to 1.0)
        double prob = DataTable::calculate_probability(coeffs, 20, Gender::male, Region::England);
        ASSERT_DOUBLE_EQ(1.0, prob); // (1 + 0.05*20) = 2.0 clamped to 1.0

        // Test with small values (not clamped)
        coeffs.age_coefficient = 0.01;
        prob = DataTable::calculate_probability(coeffs, 10, Gender::male, Region::England);
        ASSERT_NEAR(1.1, prob, 0.01); // (1 + 0.01*10) = 1.1

        // Test with negative age coefficient (clamped to 0.0)
        coeffs.age_coefficient = -0.1;
        prob = DataTable::calculate_probability(coeffs, 20, Gender::male, Region::England);
        ASSERT_DOUBLE_EQ(0.0, prob); // (1 - 0.1*20) = -1.0 clamped to 0.0
    }

    // Test gender coefficient clamping
    {
        DataTable::DemographicCoefficients coeffs;
        coeffs.gender_coefficients[Gender::male] = 0.5; // Would result in 1.5 effect

        // Test normal case (not clamped)
        double prob = DataTable::calculate_probability(coeffs, 0, Gender::male, Region::England);
        ASSERT_DOUBLE_EQ(1.0, prob); // (1 + 0.5) = 1.5 is clamped to 1.0

        // Test with negative gender coefficient
        coeffs.gender_coefficients[Gender::male] = -0.6;
        prob = DataTable::calculate_probability(coeffs, 0, Gender::male, Region::England);
        ASSERT_DOUBLE_EQ(0.0, prob); // (1 - 0.6) = 0.4 is clamped to 0.0 due to internal clamping
    }

    // Test region coefficient clamping
    {
        DataTable::DemographicCoefficients coeffs;
        coeffs.region_coefficients[Region::England] = 0.5; // Would result in 1.5 effect

        // Test normal case (not clamped)
        double prob = DataTable::calculate_probability(coeffs, 0, Gender::unknown, Region::England);
        ASSERT_DOUBLE_EQ(1.0, prob); // (1 + 0.5) = 1.5 clamped to 1.0

        // Test with small value
        coeffs.region_coefficients[Region::England] = 0.2;
        prob = DataTable::calculate_probability(coeffs, 0, Gender::unknown, Region::England);
        ASSERT_NEAR(1.2, prob, 0.01); // (1 + 0.2) = 1.2

        // Test with negative region coefficient
        coeffs.region_coefficients[Region::England] = -0.6;
        prob = DataTable::calculate_probability(coeffs, 0, Gender::unknown, Region::England);
        ASSERT_DOUBLE_EQ(0.0, prob); // (1 - 0.6) = 0.4 is clamped to 0.0 due to internal clamping
    }

    // Test ethnicity coefficient clamping
    {
        DataTable::DemographicCoefficients coeffs;
        coeffs.ethnicity_coefficients[Ethnicity::White] = 0.5; // Would result in 1.5 effect

        // Test normal case (not clamped)
        double prob = DataTable::calculate_probability(coeffs, 0, Gender::unknown, Region::unknown,
                                                       Ethnicity::White);
        ASSERT_DOUBLE_EQ(1.0, prob); // (1 + 0.5) = 1.5 clamped to 1.0

        // Test with small value
        coeffs.ethnicity_coefficients[Ethnicity::White] = 0.2;
        prob = DataTable::calculate_probability(coeffs, 0, Gender::unknown, Region::unknown,
                                                Ethnicity::White);
        ASSERT_NEAR(1.2, prob, 0.01); // (1 + 0.2) = 1.2

        // Test with negative ethnicity coefficient
        coeffs.ethnicity_coefficients[Ethnicity::White] = -0.6;
        prob = DataTable::calculate_probability(coeffs, 0, Gender::unknown, Region::unknown,
                                                Ethnicity::White);
        ASSERT_DOUBLE_EQ(0.0, prob); // (1 - 0.6) = 0.4 clamped to 0.0
    }

    // Test combined effects
    {
        DataTable::DemographicCoefficients coeffs;
        // Combined effect of 0.4
        coeffs.age_coefficient = 0.01; // 0.01 * 10 = 0.1
        coeffs.gender_coefficients[Gender::male] = 0.1;
        coeffs.region_coefficients[Region::England] = 0.1;
        coeffs.ethnicity_coefficients[Ethnicity::White] = 0.1;

        double prob = DataTable::calculate_probability(coeffs, 10, Gender::male, Region::England,
                                                       Ethnicity::White);
        // (1 + 0.1 + 0.1 + 0.1 + 0.1) = 1.4
        ASSERT_NEAR(1.4, prob, 0.01);
    }
}

// Test error handling in demographic coefficient operations - lines 247-253, 258-265
// Created - Mahima
TEST(DataTableTest, DemographicCoefficientErrors) {
    DataTable table = create_test_table();

    // Test getting non-existent demographic coefficients
    ASSERT_THROW(table.get_demographic_coefficients("nonexistent"), std::runtime_error);

    // Test zero total probability error in ethnicity distribution
    {
        // Set up coefficients where all probabilities would be zero
        DataTable::DemographicCoefficients coeffs;
        coeffs.gender_coefficients[Gender::male] = -0.5; // Creates negative probability
        table.set_demographic_coefficients("ethnicity.probabilities", coeffs);

        // Add required columns for ethnicity distribution
        table.add(std::make_unique<StringDataTableColumn>(
            "ethnicity", std::vector<std::string>{"White", "Asian", "Black", "Others"}));
        table.add(std::make_unique<DoubleDataTableColumn>("ethnicity_prob",
                                                          std::vector<double>{0.7, 0.1, 0.1, 0.1}));

        // Trying to get distribution should throw due to zero total probability
        ASSERT_THROW(table.get_ethnicity_distribution(0, Gender::male, Region::England),
                     std::runtime_error);
    }
}

// Test loading demographic coefficients from full config - lines 321-329
// Created - Mahima
TEST(DataTableTest, LoadDemographicCoefficientsFromConfig) {
    DataTable table = create_test_table();

    // Create a complete valid JSON config
    nlohmann::json config = {
        {"modelling",
         {{"demographic_models",
           {{"region",
             {{"probabilities",
               {{"coefficients", {{"age", 0.01}, {"gender", {{"male", 0.1}, {"female", 0.2}}}}}}}}},
            {"ethnicity",
             {{"probabilities",
               {{"coefficients",
                 {{"age", 0.02}, {"gender", {{"male", 0.3}, {"female", 0.4}}}}}}}}}}}}}};

    // Load coefficients
    table.load_demographic_coefficients(config);

    // Verify region coefficients
    auto region_coeffs = table.get_demographic_coefficients("region.probabilities");
    ASSERT_DOUBLE_EQ(0.01, region_coeffs.age_coefficient);
    ASSERT_DOUBLE_EQ(0.1, region_coeffs.gender_coefficients[Gender::male]);
    ASSERT_DOUBLE_EQ(0.2, region_coeffs.gender_coefficients[Gender::female]);

    // Verify ethnicity coefficients
    auto ethnicity_coeffs = table.get_demographic_coefficients("ethnicity.probabilities");
    ASSERT_DOUBLE_EQ(0.02, ethnicity_coeffs.age_coefficient);
    ASSERT_DOUBLE_EQ(0.3, ethnicity_coeffs.gender_coefficients[Gender::male]);
    ASSERT_DOUBLE_EQ(0.4, ethnicity_coeffs.gender_coefficients[Gender::female]);

    // Test with missing modelling section
    nlohmann::json invalid_config = {{"not_modelling", {}}};
    ASSERT_NO_THROW(table.load_demographic_coefficients(invalid_config));

    // Test with missing demographic_models section
    nlohmann::json invalid_config2 = {{"modelling", {{"not_demographic_models", {}}}}};
    ASSERT_NO_THROW(table.load_demographic_coefficients(invalid_config2));
}

// Additional tests for calculate_probability edge cases - lines 287-307
// Created - Mahima
TEST(DataTableTest, CalculateProbabilityEdgeCases) {
    // Test application of all coefficients together
    DataTable::DemographicCoefficients coeffs;
    coeffs.age_coefficient = 0.01;
    coeffs.gender_coefficients[Gender::male] = 0.1;
    coeffs.region_coefficients[Region::England] = 0.2;
    coeffs.ethnicity_coefficients[Ethnicity::White] = 0.3;

    // Calculate with all coefficients
    double prob = DataTable::calculate_probability(coeffs, 10, Gender::male, Region::England,
                                                   Ethnicity::White);

    // Expected: 0.01*10 + 0.1 + 0.2 + 0.3 = 0.1 + 0.1 + 0.2 + 0.3 = 0.7
    ASSERT_DOUBLE_EQ(0.7, prob);

    // Test with no coefficients (empty maps)
    DataTable::DemographicCoefficients empty_coeffs;
    empty_coeffs.age_coefficient = 0.0;

    double prob_empty = DataTable::calculate_probability(empty_coeffs, 10, Gender::male,
                                                         Region::England, Ethnicity::White);
    ASSERT_DOUBLE_EQ(0.0, prob_empty); // Should be base 0.0 with no adjustments

    // Test with missing values in coefficient maps
    DataTable::DemographicCoefficients sparse_coeffs;
    sparse_coeffs.gender_coefficients[Gender::female] = 0.1;      // Only female coefficient
    sparse_coeffs.region_coefficients[Region::Wales] = 0.2;       // Only Wales coefficient
    sparse_coeffs.ethnicity_coefficients[Ethnicity::Asian] = 0.3; // Only Asian coefficient

    // Calculate with male, England, White - none of which have coefficients
    double prob_missing = DataTable::calculate_probability(sparse_coeffs, 0, Gender::male,
                                                           Region::England, Ethnicity::White);
    ASSERT_DOUBLE_EQ(0.0, prob_missing); // Should be base 0.0 with no adjustments
}

// Tests for set_demographic_coefficients method - line 326
// Created - Mahima
TEST(DataTableTest, SetDemographicCoefficients) {
    DataTable table = create_test_table();

    // Create test coefficients
    DataTable::DemographicCoefficients coeffs1;
    coeffs1.age_coefficient = 0.1;

    // Set coefficients for first model
    table.set_demographic_coefficients("model1", coeffs1);

    // Create different coefficients
    DataTable::DemographicCoefficients coeffs2;
    coeffs2.age_coefficient = 0.2;

    // Set coefficients for second model
    table.set_demographic_coefficients("model2", coeffs2);

    // Verify coefficients are stored separately
    auto retrieved1 = table.get_demographic_coefficients("model1");
    auto retrieved2 = table.get_demographic_coefficients("model2");

    ASSERT_DOUBLE_EQ(0.1, retrieved1.age_coefficient);
    ASSERT_DOUBLE_EQ(0.2, retrieved2.age_coefficient);

    // Update existing coefficients
    DataTable::DemographicCoefficients updated;
    updated.age_coefficient = 0.3;

    // Set updated coefficients for first model
    table.set_demographic_coefficients("model1", updated);

    // Verify update worked
    auto retrieved_updated = table.get_demographic_coefficients("model1");
    ASSERT_DOUBLE_EQ(0.3, retrieved_updated.age_coefficient);
}
