#pragma once

#include "HealthGPS.Core/identifier.h"
#include "person.h"
#include "runtime_context.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace hgps {

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

    /// @brief MAHIMA: Output file stream for baseline scenario data
    std::ofstream baseline_file_;

    /// @brief MAHIMA: Output file stream for intervention scenario data
    std::ofstream intervention_file_;

    /// @brief MAHIMA: Flag to ensure Year 3 data is captured only once per scenario
    bool year_3_captured_;

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
