#include "risk_factor_inspector.h"
#include "HealthGPS.Core/exception.h"

#include <chrono>     // MAHIMA: Include for timestamp generation
#include <filesystem> // MAHIMA: Include for filesystem operations
#include <iomanip>
#include <iostream>
#include <sstream>

namespace hgps {

// MAHIMA: TOGGLE FOR YEAR 3 RISK FACTOR INSPECTION
// Must match the toggle in static_linear_model.cpp, simulation.cpp, and runtime_context.cpp
static constexpr bool ENABLE_YEAR3_RISK_FACTOR_INSPECTION = false;

RiskFactorInspector::RiskFactorInspector(const std::filesystem::path &output_dir)
    : year_3_captured_(false), output_dir_(output_dir) {

    // MAHIMA: Initialize target risk factors for inspection
    // These are the specific nutrients/risk factors that were producing
    // weird/incorrect values after policy application in Year 3
    target_factors_ = {
        // Primary nutrients of concern
        core::Identifier{"FoodAddedSugar"},   // Added Sugar
        core::Identifier{"FoodCarbohydrate"}, // Carbohydrate
        core::Identifier{"FoodFat"},          // Fat
        core::Identifier{"FoodProtein"},      // Protein

        // Fat subtypes that were showing issues
        core::Identifier{"FoodMonounsaturatedFat"},       // Monounsaturated Fat
        core::Identifier{"FoodPolyunsaturatedFattyAcid"}, // Polyunsaturated Fatty Acid
        core::Identifier{"FoodSaturatedFat"},             // Saturated Fat

        // Sugar types and vegetables
        core::Identifier{"FoodTotalSugar"}, // Total Sugar
        core::Identifier{"FoodVegetable"},  // Vegetable

        // Additional context factors for validation
        core::Identifier{"BMI"},    // Body Mass Index
        core::Identifier{"Weight"}, // Weight (kg)
        core::Identifier{"Height"}  // Height (cm)
    };

    // MAHIMA: Create timestamped output file paths
    // Using current time to ensure unique filenames for each simulation run
    auto timestamp = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S");
    std::string timestamp_str = ss.str();

    // MAHIMA: Only create Year 3 files if the feature is enabled
    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        auto baseline_path =
            output_dir / ("Year3_Baseline_Individual_RiskFactors_" + timestamp_str + ".csv");
        auto intervention_path =
            output_dir / ("Year3_Intervention_Individual_RiskFactors_" + timestamp_str + ".csv");

        // MAHIMA: Open output file streams with error checking
        // These files will contain individual person records for inspection
        baseline_file_.open(baseline_path, std::ios::out);
        intervention_file_.open(intervention_path, std::ios::out);

        if (!baseline_file_.is_open()) {
            throw core::HgpsException("MAHIMA: Failed to create baseline inspection file: " +
                                      baseline_path.string());
        }

        if (!intervention_file_.is_open()) {
            throw core::HgpsException("MAHIMA: Failed to create intervention inspection file: " +
                                      intervention_path.string());
        }
    }

    // MAHIMA: Write CSV headers immediately to both files (only if Year 3 inspection is enabled)
    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        write_headers();
    }

    // MAHIMA: Print confirmation of file creation for debugging
    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        auto baseline_path =
            output_dir / ("Year3_Baseline_Individual_RiskFactors_" + timestamp_str + ".csv");
        auto intervention_path =
            output_dir / ("Year3_Intervention_Individual_RiskFactors_" + timestamp_str + ".csv");
            
        std::cout << "\nMAHIMA: Risk Factor Inspector initialized successfully:";
        std::cout << "\n  Baseline file: " << baseline_path.string();
        std::cout << "\n  Intervention file: " << intervention_path.string();
        std::cout << "\n  Target risk factors: " << target_factors_.size() << " factors";
        std::cout << "\n  Waiting for Year 3 data capture...\n";
    }
}

RiskFactorInspector::~RiskFactorInspector() {
    // MAHIMA: Ensure files are properly closed and flushed
    if (baseline_file_.is_open()) {
        baseline_file_.flush();
        baseline_file_.close();
    }

    if (intervention_file_.is_open()) {
        intervention_file_.flush();
        intervention_file_.close();
    }

    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        std::cout << "\nMAHIMA: Risk Factor Inspector cleanup completed.\n";
    }
}

bool RiskFactorInspector::should_capture_year_3(RuntimeContext &context) {
    // MAHIMA: Check if this is Year 3 and we haven't captured data yet
    // Year 3 is when policies start being applied (time_now - start_time == 2)
    int elapsed_years = context.time_now() - context.start_time();
    bool is_year_3 = (elapsed_years == 2);

    // MAHIMA: Only capture once per scenario to avoid duplicate data
    if (is_year_3 && !year_3_captured_) {
        if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
            std::cout << "\nMAHIMA: Year 3 detected! Time to capture risk factor data.";
            std::cout << "\n  Current time: " << context.time_now();
            std::cout << "\n  Start time: " << context.start_time();
            std::cout << "\n  Elapsed years: " << elapsed_years;
            std::cout << "\n  Scenario type: "
                      << (context.scenario().type() == ScenarioType::baseline ? "Baseline"
                                                                              : "Intervention");
        }
        return true;
    }

    return false;
}

void RiskFactorInspector::capture_year_3_data(RuntimeContext &context) {
    // MAHIMA: Determine which scenario we're capturing data for
    std::string scenario_type;
    std::ofstream *target_file;

    if (context.scenario().type() == ScenarioType::baseline) {
        scenario_type = "Baseline";
        target_file = &baseline_file_;
        if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
            std::cout << "\nMAHIMA: Capturing Year 3 BASELINE risk factor data...";
        }
    } else {
        scenario_type = "Intervention";
        target_file = &intervention_file_;
        if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
            std::cout
                << "\nMAHIMA: Capturing Year 3 INTERVENTION risk factor data (post-policy)...";
        }
    }

    // MAHIMA: Count active population for progress tracking
    size_t active_count = 0;
    size_t total_count = 0;

    for (const auto &person : context.population()) {
        total_count++;
        if (person.is_active()) {
            active_count++;
        }
    }

    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        std::cout << "\n  Population to process: " << active_count << " active out of "
                  << total_count << " total";
    }

    // MAHIMA: Write individual person records to the appropriate file
    size_t records_written = 0;
    for (const auto &person : context.population()) {
        // MAHIMA: Skip inactive people (dead, emigrated, etc.)
        if (!person.is_active()) {
            continue;
        }

        // MAHIMA: Write this person's complete risk factor profile
        write_person_record(*target_file, person, scenario_type, context.current_run(),
                            context.time_now());
        records_written++;
    }

    // MAHIMA: Flush the file to ensure data is written immediately
    target_file->flush();

    // MAHIMA: Mark as captured to prevent duplicate data
    year_3_captured_ = true;

    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        std::cout << "\nMAHIMA: Year 3 " << scenario_type << " data capture completed!";
        std::cout << "\n  Records written: " << records_written;
        std::cout << "\n  File flushed and ready for inspection.\n";
    }
}

void RiskFactorInspector::write_headers() {
    // MAHIMA: Create comprehensive CSV header row
    // Format: source,run,time,person_id,age,gender,region,ethnicity,income,
    //         [all target risk factors],
    //         bmi,weight,height

    std::string header = "source,run,time,person_id,age,gender,region,ethnicity,income,";

    // MAHIMA: Add each target risk factor as a column
    // Using simplified names (removing "Food" prefix for readability)
    header += "addedsugar,carbohydrate,fat,protein,";
    header += "monounsaturatedfat,polyunsaturatedfattyacid,saturatedfat,";
    header += "totalsugar,vegetable,";
    header += "bmi,weight,height";

    header += "\n";

    // MAHIMA: Write headers to both files
    baseline_file_ << header;
    intervention_file_ << header;

    // MAHIMA: Flush headers immediately so they're visible even if simulation crashes
    baseline_file_.flush();
    intervention_file_.flush();

    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        std::cout << "\nMAHIMA: CSV headers written to both inspection files.";
    }
}

void RiskFactorInspector::write_person_record(std::ofstream &file, const Person &person,
                                              const std::string &source, int run, int time) {
    // MAHIMA: Write basic identification and demographic information
    file << source << "," << run << "," << time << "," << person.id() << "," << person.age << ",";

    // MAHIMA: Write gender (convert enum to string)
    file << (person.gender == core::Gender::male ? "male" : "female") << ",";

    // MAHIMA: Write demographic information for context
    // These help identify which population groups are most affected by policies
    file << region_enum_to_string(person.region) << ",";
    file << ethnicity_enum_to_string(person.ethnicity) << ",";
    file << person.income_continuous << ",";

    // MAHIMA: Write all target risk factor values with safe error handling
    // This is the core data we need to inspect for outliers and weird values
    for (size_t i = 0; i < target_factors_.size(); i++) {
        const auto &factor = target_factors_[i];

        // MAHIMA: Get the risk factor value safely (handles missing/NaN values)
        std::string value_str = get_safe_risk_factor_value(person, factor);
        file << value_str;

        // MAHIMA: Add comma separator except for the last column
        if (i < target_factors_.size() - 1) {
            file << ",";
        }
    }

    // MAHIMA: End the record with a newline
    file << "\n";
}

std::string RiskFactorInspector::get_safe_risk_factor_value(const Person &person,
                                                            const core::Identifier &factor) {
    // MAHIMA: Safely extract risk factor value with comprehensive error handling
    // This is crucial for identifying problematic values that are causing issues

    try {
        // MAHIMA: Check if the risk factor exists for this person
        if (!person.risk_factors.contains(factor)) {
            return "MISSING"; // Mark missing risk factors for easy identification
        }

        // MAHIMA: Get the actual value
        double value = person.risk_factors.at(factor);

        // MAHIMA: Check for problematic values (NaN, infinity, etc.)
        if (std::isnan(value)) {
            return "NaN"; // Not a Number - indicates calculation error
        }

        if (std::isinf(value)) {
            return "INF"; // Infinite value - indicates division by zero or overflow
        }

        if (value < 0.0) {
            // MAHIMA: Negative values might be problematic for nutrients
            // Convert to string but mark as potentially suspicious
            return std::to_string(value) + "_NEG";
        }

        // MAHIMA: Normal value - convert to string with reasonable precision
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << value;
        return oss.str();

    } catch (const std::exception &) {
        // MAHIMA: Catch any other errors and mark them for investigation
        return "ERROR";
    }
}

// MAHIMA: Helper method to convert region enum to string
// This provides human-readable region names for the CSV output
std::string RiskFactorInspector::region_enum_to_string(core::Region region) {
    switch (region) {
    case core::Region::England:
        return "England";
    case core::Region::Wales:
        return "Wales";
    case core::Region::Scotland:
        return "Scotland";
    case core::Region::NorthernIreland:
        return "NorthernIreland";
    case core::Region::unknown:
    default:
        return "Unknown";
    }
}

// MAHIMA: Helper method to convert ethnicity enum to string
// This provides human-readable ethnicity names for the CSV output
std::string RiskFactorInspector::ethnicity_enum_to_string(core::Ethnicity ethnicity) {
    switch (ethnicity) {
    case core::Ethnicity::White:
        return "White";
    case core::Ethnicity::Black:
        return "Black";
    case core::Ethnicity::Asian:
        return "Asian";
    case core::Ethnicity::Mixed:
        return "Mixed";
    case core::Ethnicity::Other:
        return "Other";
    case core::Ethnicity::unknown:
    default:
        return "Unknown";
    }
}

// MAHIMA: Set debug configuration for detailed calculation capture
void RiskFactorInspector::set_debug_config(bool enabled, int age, core::Gender gender, 
                                          const std::string &risk_factor) {
    debug_config_.enabled = enabled;
    debug_config_.target_age = age;
    debug_config_.target_gender = gender;
    debug_config_.target_risk_factor = risk_factor;
    
    if (enabled) {
        std::cout << "\nMAHIMA: Debug configuration set - Age: " << age 
                  << ", Gender: " << (gender == core::Gender::male ? "male" : 
                                     gender == core::Gender::female ? "female" : "any")
                  << ", Risk Factor: " << (risk_factor.empty() ? "any" : risk_factor);
    }
}

// MAHIMA: Capture detailed risk factor calculation steps for debugging
void RiskFactorInspector::capture_detailed_calculation(RuntimeContext &context, const Person &person, 
                                                      const std::string &risk_factor_name, size_t risk_factor_index,
                                                      double random_residual_before_cholesky, double residual_after_cholesky,
                                                      double expected_value, double linear_result, double residual,
                                                      double stddev, double combined, double lambda, double boxcox_result,
                                                      double factor_before_clamp, double range_lower, double range_upper,
                                                      double final_clamped_factor) {
    
    // Check if debugging is enabled and this person/risk factor matches our criteria
    if (!debug_config_.enabled) {
        return;
    }
    
    // Check age filter
    if (debug_config_.target_age != -1 && person.age != static_cast<unsigned int>(debug_config_.target_age)) {
        return;
    }
    
    // Check gender filter
    if (debug_config_.target_gender != core::Gender::unknown && person.gender != debug_config_.target_gender) {
        return;
    }
    
    // Check risk factor filter
    if (!debug_config_.target_risk_factor.empty() && 
        !core::case_insensitive::equals(risk_factor_name, debug_config_.target_risk_factor)) {
        return;
    }
    
    // Create filename: {risk_factor_name}_inspection.csv
    std::string filename = core::to_lower(risk_factor_name) + "_inspection.csv";
    std::filesystem::path file_path = output_dir_ / filename;
    
    // Check if file exists to determine if we need to write headers
    bool file_exists = std::filesystem::exists(file_path);
    
    // Open file in append mode
    std::ofstream file(file_path, std::ios::app);
    if (!file.is_open()) {
        std::cout << "\nERROR: Could not open debug file: " << file_path.string();
        return;
    }
    
    // Write headers if this is a new file
    if (!file_exists) {
        file << "person_id,gender,age,region,ethnicity,physical_activity,income_continuous,income_category,"
             << "random_residual_before_cholesky,residual_after_cholesky,RF_value,expected_value,linear_result,"
             << "residual,stddev,combined,lambda,boxcox_result,factor_before_clamp,range_lower,range_upper,"
             << "final_clamped_factor\n";
    }
    
    // Write person data
    file << person.id() << ","
         << (person.gender == core::Gender::male ? "male" : "female") << ","
         << person.age << ","
         << region_enum_to_string(person.region) << ","
         << ethnicity_enum_to_string(person.ethnicity) << ","
         << person.physical_activity << ","
         << person.income_continuous << ","
         << person.income_category << ","
         << random_residual_before_cholesky << ","
         << residual_after_cholesky << ","
         << risk_factor_name << ","
         << expected_value << ","
         << linear_result << ","
         << residual << ","
         << stddev << ","
         << combined << ","
         << lambda << ","
         << boxcox_result << ","
         << factor_before_clamp << ","
         << range_lower << ","
         << range_upper << ","
         << final_clamped_factor << "\n";
    
    file.close();
}

// MAHIMA: Store calculation details for later capture
void RiskFactorInspector::store_calculation_details(const Person &person, const std::string &risk_factor_name, size_t risk_factor_index,
                                                   double random_residual_before_cholesky, double residual_after_cholesky,
                                                   double expected_value, double linear_result, double residual,
                                                   double stddev, double combined, double lambda, double boxcox_result,
                                                   double factor_before_clamp, double range_lower, double range_upper,
                                                   double final_clamped_factor) {
    if (!debug_config_.enabled) {
        return;
    }

    // Check if this person and risk factor should be captured
    bool should_capture = true;
    
    if (debug_config_.target_age != -1 && person.age != debug_config_.target_age) {
        should_capture = false;
    }
    
    if (debug_config_.target_gender != core::Gender::unknown && person.gender != debug_config_.target_gender) {
        should_capture = false;
    }
    
    if (!debug_config_.target_risk_factor.empty() && risk_factor_name != debug_config_.target_risk_factor) {
        should_capture = false;
    }
    
    // Debug output removed for cleaner console output
    
    if (!should_capture) {
        return;
    }

    // Store the calculation details
    std::string person_id = std::to_string(person.id());
    CalculationDetails details;
    details.random_residual_before_cholesky = random_residual_before_cholesky;
    details.residual_after_cholesky = residual_after_cholesky;
    details.expected_value = expected_value;
    details.linear_result = linear_result;
    details.residual = residual;
    details.stddev = stddev;
    details.combined = combined;
    details.lambda = lambda;
    details.boxcox_result = boxcox_result;
    details.factor_before_clamp = factor_before_clamp;
    details.range_lower = range_lower;
    details.range_upper = range_upper;
    details.final_clamped_factor = final_clamped_factor;
    
    calculation_storage_[person_id][risk_factor_name] = details;
}

// MAHIMA: Capture person risk factors after all calculations are complete
void RiskFactorInspector::capture_person_risk_factors(RuntimeContext &context, const Person &person, 
                                                     const std::string &risk_factor_name, size_t risk_factor_index) {
    if (!debug_config_.enabled) {
        return;
    }

    // Check if this person and risk factor should be captured
    bool should_capture = true;
    
    if (debug_config_.target_age != -1 && person.age != debug_config_.target_age) {
        should_capture = false;
    }
    
    if (debug_config_.target_gender != core::Gender::unknown && person.gender != debug_config_.target_gender) {
        should_capture = false;
    }
    
    if (!debug_config_.target_risk_factor.empty() && risk_factor_name != debug_config_.target_risk_factor) {
        should_capture = false;
    }
    
    if (!should_capture) {
        return;
    }

    // Count how many people match our criteria (no console output)
    static int match_count = 0;
    match_count++;

    std::string person_id = std::to_string(person.id());
    
    // Check if we have stored calculation details for this person and risk factor
    if (calculation_storage_.find(person_id) == calculation_storage_.end() ||
        calculation_storage_[person_id].find(risk_factor_name) == calculation_storage_[person_id].end()) {
        return;
    }
    
    const auto& details = calculation_storage_[person_id][risk_factor_name];
    
    // Create the CSV file path (only baseline scenario runs debugging)
    std::filesystem::path csv_path = output_dir_ / (risk_factor_name + "_inspection.csv");
    
    // Check if file exists to determine if we need to write headers
    bool file_exists = std::filesystem::exists(csv_path);
    
    // Open file in append mode
    std::ofstream file(csv_path, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "\nMAHIMA: Error opening file for writing: " << csv_path << std::endl;
        return;
    }
    
    // Write headers if file is new
    if (!file_exists) {
        file << "person_id,gender,age,region,ethnicity,physical_activity,income_continuous,income_category,"
             << "random_residual_before_cholesky,residual_after_cholesky,RF_value,expected_value,linear_result,"
             << "residual,stddev,combined,lambda,boxcox_result,factor_before_clamp,range_lower,range_upper,"
             << "final_clamped_factor\n";
    }
    
    // Write the data
    file << person_id << ","
         << (person.gender == core::Gender::male ? "male" : "female") << ","
         << person.age << ","
         << (person.region == core::Region::England ? "England" : 
             person.region == core::Region::Scotland ? "Scotland" : 
             person.region == core::Region::Wales ? "Wales" : "NorthernIreland") << ","
         << (person.ethnicity == core::Ethnicity::White ? "White" :
             person.ethnicity == core::Ethnicity::Black ? "Black" :
             person.ethnicity == core::Ethnicity::Asian ? "Asian" :
             person.ethnicity == core::Ethnicity::Mixed ? "Mixed" : "Other") << ","
         << person.physical_activity << ","
         << person.income_continuous << ","
         << static_cast<int>(person.income) << ","
         << details.random_residual_before_cholesky << ","
         << details.residual_after_cholesky << ","
         << risk_factor_name << ","
         << details.expected_value << ","
         << details.linear_result << ","
         << details.residual << ","
         << details.stddev << ","
         << details.combined << ","
         << details.lambda << ","
         << details.boxcox_result << ","
         << details.factor_before_clamp << ","
         << details.range_lower << ","
         << details.range_upper << ","
         << details.final_clamped_factor << "\n";
    
    file.close();
    
    // Count records written and show progress
    static int total_written = 0;
    total_written++;
    
    // Show progress every 50 records
    if (total_written % 200 == 0 || total_written == 1) {
        std::cout << "\nMAHIMA: Written " << total_written << " records to " << csv_path.filename().string();
    }
}

// MAHIMA: Analyze population and count people matching debug criteria
void RiskFactorInspector::analyze_population_demographics(RuntimeContext &context) {
    if (!debug_config_.enabled) {
        return;
    }
    
    int total_population = context.population().size();
    int matching_age_gender = 0;
    int matching_age_gender_risk_factor = 0;
    
    // Count people matching age and gender criteria
    for (const auto &person : context.population()) {
        bool age_matches = (debug_config_.target_age == -1) || (person.age == debug_config_.target_age);
        bool gender_matches = (debug_config_.target_gender == core::Gender::unknown) || (person.gender == debug_config_.target_gender);
        
        if (age_matches && gender_matches) {
            matching_age_gender++;
            
            // Check if this person has the target risk factor
            if (!debug_config_.target_risk_factor.empty()) {
                auto risk_factor_id = core::Identifier(debug_config_.target_risk_factor);
                if (person.risk_factors.find(risk_factor_id) != person.risk_factors.end()) {
                    matching_age_gender_risk_factor++;
                }
            }
        }
    }
    
    // Show population analysis results
    std::cout << "\nMAHIMA: Population analysis - Total: " << total_population 
              << ", Matching " << debug_config_.target_age << "-year-old " 
              << (debug_config_.target_gender == core::Gender::male ? "males" : "females")
              << " for " << debug_config_.target_risk_factor << ": " << matching_age_gender_risk_factor << " people";
}

} // namespace hgps
