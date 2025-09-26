#pragma once

#include "HealthGPS.Core/identifier.h"
#include "person.h"
#include "runtime_context.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace hgps {

/// @brief MAHIMA: Configuration for detailed calculation debugging
/// @details User can specify age range, gender, risk factor, and year to debug
struct DebugConfig {
    bool enabled = false;
    int min_age = -1;  // -1 means no minimum age limit
    int max_age = -1;  // -1 means no maximum age limit
    core::Gender target_gender = core::Gender::unknown;  // unknown means any gender
    std::string target_risk_factor = "";  // empty means any risk factor
    int target_year = -1;  // -1 means any year, otherwise specific year (e.g., 2032)
    std::string target_scenario = "";  // empty means any scenario, "baseline" or "intervention"
    
    // Helper method to check if an age is within the range
    bool is_age_in_range(int age) const {
        if (min_age == -1 && max_age == -1) return true;  // No age restrictions
        if (min_age == -1) return age <= max_age;  // Only max age specified
        if (max_age == -1) return age >= min_age;  // Only min age specified
        return age >= min_age && age <= max_age;  // Both min and max specified
    }
    
    // Helper method to check if a year matches the target
    bool is_year_match(int current_year) const {
        if (target_year == -1) return true;  // No year restriction
        return current_year == target_year;
    }
    
    // Helper method to check if a scenario matches the target
    bool is_scenario_match(const std::string& current_scenario) const {
        if (target_scenario.empty()) return true;  // No scenario restriction
        return current_scenario == target_scenario;
    }
};

/// @brief MAHIMA: Risk Factor Inspector for Year 3 Policy Application Analysis
///
/// @details This class captures individual person risk factor values in Year 3
/// (when policies are applied) to help identify outliers and inspect any
/// incorrect/weird values after policy application. The class creates separate
/// CSV files for baseline and intervention scenarios with individual person records.
///
/// Purpose: Debug policy effects on specific nutrients/risk factors
/// Target: Year 3 data capture (when policy application begins)
/// Output: Individual person records with exact risk factor values
class RiskFactorInspector {
  public:
    /// @brief MAHIMA: Initialize the Risk Factor Inspector
    /// @param output_dir Directory where the inspection CSV files will be created
    /// @throws std::runtime_error if output files cannot be created
    explicit RiskFactorInspector(const std::filesystem::path &output_dir);

    /// @brief MAHIMA: Destructor ensures files are properly closed
    ~RiskFactorInspector();

    // Delete copy constructor and assignment operator to prevent issues with file streams
    RiskFactorInspector(const RiskFactorInspector &) = delete;
    RiskFactorInspector &operator=(const RiskFactorInspector &) = delete;

    /// @brief MAHIMA: Check if we should capture Year 3 data
    /// @param context Runtime context containing simulation time information
    /// @return true if this is Year 3 and data hasn't been captured yet
    bool should_capture_year_3(RuntimeContext &context);

    /// @brief MAHIMA: Capture Year 3 individual risk factor data
    /// @param context Runtime context containing population and scenario information
    ///
    /// @details This method iterates through all active people in the population
    /// and writes their individual risk factor values to the appropriate CSV file
    /// (baseline or intervention). Called after policies have been applied.
    void capture_year_3_data(RuntimeContext &context);

    /// @brief MAHIMA: Set debug configuration for detailed calculation capture
    /// @param enabled Whether to enable detailed debugging
    /// @param min_age Minimum age to debug (-1 for no minimum)
    /// @param max_age Maximum age to debug (-1 for no maximum)
    /// @param gender Target gender to debug (unknown for any gender)
    /// @param risk_factor Target risk factor to debug (empty for any risk factor)
    /// @param target_year Target year to debug (-1 for any year)
    void set_debug_config(bool enabled, int min_age = -1, int max_age = -1, 
                         core::Gender gender = core::Gender::unknown, 
                         const std::string &risk_factor = "", int target_year = -1,
                         const std::string &target_scenario = "");
    
    /// @brief MAHIMA: Set debug configuration for single age (convenience method)
    /// @param enabled Whether to enable detailed debugging
    /// @param age Single age to debug (-1 for any age)
    /// @param gender Target gender to debug (unknown for any gender)
    /// @param risk_factor Target risk factor to debug (empty for any risk factor)
    /// @param target_year Target year to debug (-1 for any year)
    void set_debug_config_single_age(bool enabled, int age = -1, 
                                   core::Gender gender = core::Gender::unknown, 
                                   const std::string &risk_factor = "", int target_year = -1,
                                   const std::string &target_scenario = "");
    
    /// @brief MAHIMA: Add multiple debug configurations for different capture scenarios
    /// @param configs Vector of debug configurations to add
    void add_debug_configs(const std::vector<DebugConfig> &configs);
    
    /// @brief MAHIMA: Add a single debug configuration (convenience method)
    /// @param enabled Whether debugging is enabled
    /// @param min_age Minimum age to capture (or -1 for no minimum)
    /// @param max_age Maximum age to capture (or -1 for no maximum)
    /// @param gender Target gender to capture (or unknown for any)
    /// @param risk_factor Target risk factor to capture (or empty for any)
    /// @param target_year Target year to capture (or -1 for any year)
    /// @param target_scenario Target scenario to capture (or empty for any)
    void add_debug_config(bool enabled, int min_age = -1, int max_age = -1,
                          core::Gender gender = core::Gender::unknown,
                          const std::string &risk_factor = "", int target_year = -1,
                          const std::string &target_scenario = "");
    
    /// @brief MAHIMA: Clear all debug configurations
    void clear_debug_configs();
    
    /// @brief MAHIMA: Check if debug is enabled
    bool is_debug_enabled() const;
    
    /// @brief MAHIMA: Get target risk factor from debug config
    const std::string& get_target_risk_factor() const;
    
    /// @brief MAHIMA: Get all debug configurations
    const std::vector<DebugConfig>& get_debug_configs() const;
    
    /// @brief MAHIMA: Get current scenario name from context
    std::string get_current_scenario_name(RuntimeContext &context) const;
    
    /// @brief MAHIMA: Get BMI value for a person (calculated or stored)
    std::string get_bmi_value(const Person &person);

    /// @brief MAHIMA: Capture detailed risk factor calculation steps for debugging
    /// @param context Runtime context containing population and scenario information
    /// @param person Individual person whose calculation to capture
    /// @param risk_factor_name Name of the risk factor to debug
    /// @param risk_factor_index Index of the risk factor in the calculation
    /// @param random_residual_before_cholesky Random residual before Cholesky transformation
    /// @param residual_after_cholesky Residual after Cholesky transformation
    /// @param expected_value Expected value for this risk factor
    /// @param linear_result Linear model result
    /// @param residual Final residual value
    /// @param stddev Standard deviation for this risk factor
    /// @param combined Combined value before BoxCox
    /// @param lambda Lambda value for BoxCox transformation
    /// @param boxcox_result Result after BoxCox transformation
    /// @param factor_before_clamp Value before range clamping
    /// @param range_lower Lower bound of the range
    /// @param range_upper Upper bound of the range
    /// @param final_clamped_factor Final value after range clamping
    /// @param simulated_mean Mean value for this age/gender group
    /// @param factors_mean_delta Adjustment delta (expected - simulated_mean)
    /// @param final_factor_value_after_adjustment Value after adding delta
    /// @param final_value_after_everything Final value after all processing
    ///
    /// @details This method captures detailed calculation steps for a specific risk factor
    /// and writes them to a CSV file named {risk_factor_name}_inspection.csv
    void capture_detailed_calculation(RuntimeContext &context, const Person &person, 
                                    const std::string &risk_factor_name, size_t risk_factor_index,
                                    double random_residual_before_cholesky, double residual_after_cholesky,
                                    double expected_value, double linear_result, double residual,
                                    double stddev, double combined, double lambda, double boxcox_result,
                                    double factor_before_clamp, double range_lower, double range_upper,
                                    double first_clamped_factor_value, double simulated_mean = 0.0,
                                    double factors_mean_delta = 0.0, double final_factor_value_after_adjustment = 0.0,
                                    double final_value_after_everything = 0.0);

    // MAHIMA: Capture person risk factors after all calculations are complete
    void capture_person_risk_factors(RuntimeContext &context, const Person &person, 
                                   const std::string &risk_factor_name, size_t risk_factor_index);

    // MAHIMA: Store calculation details for later capture
    void store_calculation_details(const Person &person, const std::string &risk_factor_name, size_t risk_factor_index,
                                 double random_residual_before_cholesky, double residual_after_cholesky,
                                 double expected_value, double linear_result, double residual,
                                 double stddev, double combined, double lambda, double boxcox_result,
                                 double factor_before_clamp, double range_lower, double range_upper,
                                 double first_clamped_factor_value, double simulated_mean = 0.0, 
                                 double factors_mean_delta = 0.0, double value_after_adjustment_before_second_clamp = 0.0,
                                 double final_value_after_second_clamp = 0.0);

    // MAHIMA: Analyze population and count people matching debug criteria
    void analyze_population_demographics(RuntimeContext &context);
    
    // MAHIMA: Update stored calculation details with adjustment values
    void update_calculation_details_with_adjustments(const Person &person, const std::string &risk_factor_name,
                                                   double expected_value, double simulated_mean, double factors_mean_delta,
                                                   double value_after_adjustment_before_second_clamp,
                                                   double final_value_after_second_clamp);

    /// @brief MAHIMA: Structure to store calculation details for each person
    struct CalculationDetails {
        double random_residual_before_cholesky;
        double residual_after_cholesky;
        double expected_value;
        double linear_result;
        double residual;
        double stddev;
        double combined;
        double lambda;
        double boxcox_result;
        double factor_before_clamp;
        double range_lower;
        double range_upper;
        double first_clamped_factor_value;
        double simulated_mean;                        // Mean value for this age/gender group
        double factors_mean_delta;                    // Adjustment delta (expected - simulated_mean)
        double value_after_adjustment_before_second_clamp;  // Value after adding delta but before second clamp
        double final_value_after_second_clamp;       // Final value after second clamp (final value)
    };

    /// @brief MAHIMA: Get stored calculation details for a person and risk factor
    /// @param person The person to get details for
    /// @param risk_factor_name The risk factor name
    /// @param details Output parameter to store the details
    /// @return true if details were found and stored, false otherwise
    bool get_stored_calculation_details(const Person &person, const std::string &risk_factor_name, CalculationDetails &details);

    /// @brief MAHIMA: Update BMI values in stored calculation details after BMI calculation
    /// @param person The person whose BMI was just calculated
    /// @details This method updates the BMI value in all stored calculation details
    /// for the given person after the Kevin Hall model has calculated the final BMI
    void update_bmi_in_stored_details(const Person &person);

  private:
    /// @brief MAHIMA: Target risk factors to capture for inspection
    ///
    /// @details These are the specific nutrients/risk factors that were identified
    /// as producing weird/incorrect values after policy application:
    /// - Added Sugar, Carbohydrate, Fat, Protein
    /// - Monounsaturated Fat, Polyunsaturated Fatty Acid, Saturated Fat
    /// - Total Sugar, Vegetable
    /// Plus BMI, Weight, Height for context
    std::vector<core::Identifier> target_factors_;

private:
    
    /// @brief MAHIMA: Multiple debug configurations for different capture scenarios
    std::vector<DebugConfig> debug_configs_;

    /// @brief MAHIMA: Output file stream for baseline scenario data
    std::ofstream baseline_file_;

    /// @brief MAHIMA: Output file stream for intervention scenario data
    std::ofstream intervention_file_;

    /// @brief MAHIMA: Flag to ensure Year 3 data is captured only once per scenario
    bool year_3_captured_;

    /// @brief MAHIMA: Output directory for inspection files
    std::filesystem::path output_dir_;

    /// @brief MAHIMA: Map to store calculation details by person ID and risk factor
    std::unordered_map<std::string, std::unordered_map<std::string, CalculationDetails>> calculation_storage_;
    
    /// @brief MAHIMA: Counter for total records written to inspection files
    int total_records_written_;

    /// @brief MAHIMA: Write CSV headers for both output files
    ///
    /// @details Creates headers with: source, run, time, person_id, age, gender,
    /// region, ethnicity, income, followed by all target risk factor columns
    void write_headers();

    /// @brief MAHIMA: Write individual person record to specified file
    /// @param file Output file stream (baseline or intervention)
    /// @param person Individual person whose data to write
    /// @param source Scenario type ("Baseline" or "Intervention")
    /// @param run Current simulation run number
    /// @param time Current simulation time (should be Year 3)
    ///
    /// @details Writes one row per person with all their risk factor values,
    /// handling missing values and NaN/infinite values gracefully
    void write_person_record(std::ofstream &file, const Person &person, const std::string &source,
                             int run, int time);

    /// @brief MAHIMA: Safely get risk factor value with error handling
    /// @param person Person to get risk factor from
    /// @param factor Risk factor identifier
    /// @return String representation of the value ("MISSING" or "NaN" for problems)
    std::string get_safe_risk_factor_value(const Person &person, const core::Identifier &factor);

    /// @brief MAHIMA: Helper method to convert region enum to string
    /// @param region Region enum value
    /// @return Human-readable region name for CSV output
    std::string region_enum_to_string(core::Region region);

    /// @brief MAHIMA: Helper method to convert ethnicity enum to string
    /// @param ethnicity Ethnicity enum value
    /// @return Human-readable ethnicity name for CSV output
    std::string ethnicity_enum_to_string(core::Ethnicity ethnicity);
};

} // namespace hgps
