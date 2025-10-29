#include "risk_factor_inspector.h"
#include "HealthGPS.Core/exception.h"

#include <chrono>     // MAHIMA: Include for timestamp generation
#include <filesystem> // MAHIMA: Include for filesystem operations
#include <iomanip>
#include <iostream>
#include <sstream>
#include <fmt/chrono.h>  // MAHIMA: Include for timestamp formatting
#include <fmt/core.h>    // MAHIMA: Include for fmt::format

namespace hgps {

// MAHIMA: TOGGLE FOR YEAR 3 RISK FACTOR INSPECTION
// Must match the toggle in static_linear_model.cpp, simulation.cpp, and runtime_context.cpp
static constexpr bool ENABLE_YEAR3_RISK_FACTOR_INSPECTION = false;

RiskFactorInspector::RiskFactorInspector(const std::filesystem::path &output_dir, int simulation_start_time)
    : year_3_captured_(false), output_dir_(output_dir), total_records_written_(0), simulation_start_time_(simulation_start_time) {

    // MAHIMA: Generate deterministic timestamp for consistent file naming across scenarios
    // Use ONLY simulation start time to create a deterministic timestamp that will be identical
    // for both baseline and intervention scenarios within the same simulation run
    // Format: simulation_start_time_YYYY-MM-DD_HH-MM-SS
    // Create a static timestamp that's generated once and reused for all instances
    static std::string static_timestamp;
    if (static_timestamp.empty()) {
        auto current_time = std::chrono::system_clock::now();
        auto base_timestamp = fmt::format("{0:%F_%H-%M-}{1:%S}", current_time, current_time.time_since_epoch());
        static_timestamp = base_timestamp;
    }
    
    // MAHIMA: Use simulation start time as the primary identifier for deterministic naming
    // This ensures both baseline and intervention scenarios use the exact same filename
    timestamp_str_ = std::to_string(simulation_start_time_) + "_" + static_timestamp;

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

    // MAHIMA: Use the pre-generated timestamp for consistent file naming

    // MAHIMA: Try to find the main simulation output folder by looking for existing result files
    // This ensures risk factor inspection files are saved in the same location as main results
    std::filesystem::path main_output_dir = output_dir;
    
    std::cout << "\nMAHIMA: Searching for main simulation output directory...";
    std::cout << "\n  Current output_dir: " << output_dir.string();
    std::cout << "\n  Current working directory: " << std::filesystem::current_path().string();
    
    // Look for existing HealthGPS result files in multiple locations
    std::vector<std::filesystem::path> search_paths = {
        output_dir,
        output_dir.parent_path(),
        output_dir.parent_path().parent_path(),  // Go up one more level
        std::filesystem::current_path(),
        std::filesystem::current_path().parent_path(),
        std::filesystem::current_path().parent_path().parent_path()  // Go up one more level
    };
    
    bool found_main_dir = false;
    for (const auto& search_path : search_paths) {
        if (std::filesystem::exists(search_path)) {
            std::cout << "\n  Checking: " << search_path.string();
            try {
                for (const auto& entry : std::filesystem::directory_iterator(search_path)) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().filename().string();
                        if (filename.find("HealthGPS_Result_") == 0 && filename.find(".csv") != std::string::npos) {
                            main_output_dir = entry.path().parent_path();
                            std::cout << "\nMAHIMA: Found main simulation output folder: " << main_output_dir.string();
                            found_main_dir = true;
                            break;
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "\n  Error accessing " << search_path.string() << ": " << e.what();
            }
            if (found_main_dir) break;
        } else {
            std::cout << "\n  Path does not exist: " << search_path.string();
        }
    }
    
    if (!found_main_dir) {
        std::cout << "\nMAHIMA: WARNING - Could not find main simulation output directory, using: " << main_output_dir.string();
    }

    // MAHIMA: Only create Year 3 files if the feature is enabled
    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        auto baseline_path =
            main_output_dir / ("Year3_Baseline_Individual_RiskFactors_" + timestamp_str_ + ".csv");
        auto intervention_path =
            main_output_dir / ("Year3_Intervention_Individual_RiskFactors_" + timestamp_str_ + ".csv");

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
            main_output_dir / ("Year3_Baseline_Individual_RiskFactors_" + timestamp_str_ + ".csv");
        auto intervention_path =
            main_output_dir / ("Year3_Intervention_Individual_RiskFactors_" + timestamp_str_ + ".csv");
            
        std::cout << "\nMAHIMA: Risk Factor Inspector initialized successfully:";
        std::cout << "\n  Main output directory: " << main_output_dir.string();
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

std::string RiskFactorInspector::get_bmi_value(const Person &person) {
    // MAHIMA: Calculate BMI on-the-fly if Weight and Height are available
    // This handles the case where BMI hasn't been calculated yet by the Kevin Hall model
    
    try {
        // Check if BMI is already calculated and stored
        if (person.risk_factors.contains("BMI"_id)) {
            double bmi = person.risk_factors.at("BMI"_id);
            if (std::isnan(bmi)) {
                return "NaN";
            }
            if (std::isinf(bmi)) {
                return "INF";
            }
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << bmi;
            return oss.str();
        }
        
        // If BMI not available, try to calculate it from Weight and Height
        if (person.risk_factors.contains("Weight"_id) && person.risk_factors.contains("Height"_id)) {
            double weight = person.risk_factors.at("Weight"_id);
            double height = person.risk_factors.at("Height"_id);
            
            if (std::isnan(weight) || std::isnan(height) || std::isinf(weight) || std::isinf(height)) {
                return "INVALID_DATA";
            }
            
            if (height <= 0.0) {
                return "INVALID_HEIGHT";
            }
            
            // Calculate BMI: weight (kg) / height (m)^2
            // Height is stored in cm, so convert to meters
            double height_m = height / 100.0;
            double bmi = weight / (height_m * height_m);
            
            if (std::isnan(bmi) || std::isinf(bmi)) {
                return "CALC_ERROR";
            }
            
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << bmi;
            return oss.str();
        }
        
        // If neither BMI nor Weight/Height are available
        return "NOT_CALCULATED_YET";
        
    } catch (const std::exception &) {
        return "ERROR";
    }
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
void RiskFactorInspector::set_debug_config(bool enabled, int min_age, int max_age, 
                                          core::Gender gender, const std::string &risk_factor, int target_year,
                                          const std::string &target_scenario) {
    // Clear existing configs and add this one
    debug_configs_.clear();
    
    DebugConfig config;
    config.enabled = enabled;
    config.min_age = min_age;
    config.max_age = max_age;
    config.target_gender = gender;
    config.target_risk_factor = risk_factor;
    config.target_year = target_year;
    config.target_scenario = target_scenario;
    
    debug_configs_.push_back(config);
    
    if (enabled) {
        std::string age_range_str;
        if (min_age == -1 && max_age == -1) {
            age_range_str = "any age";
        } else if (min_age == -1) {
            age_range_str = "age <= " + std::to_string(max_age);
        } else if (max_age == -1) {
            age_range_str = "age >= " + std::to_string(min_age);
        } else {
            age_range_str = "age " + std::to_string(min_age) + "-" + std::to_string(max_age);
        }
        
        std::string year_str = (target_year == -1) ? "any year" : "year " + std::to_string(target_year);
        std::string scenario_str = (target_scenario.empty() ? "any scenario" : target_scenario);
        
        std::cout << "\nMAHIMA: Debug configuration set - " << age_range_str
                  << ", Gender: " << (gender == core::Gender::male ? "male" : 
                                     gender == core::Gender::female ? "female" : "any")
                  << ", Risk Factor: " << (risk_factor.empty() ? "any" : risk_factor)
                  << ", Year: " << year_str
                  << ", Scenario: " << scenario_str;
        
        // Debug: Show the actual values being stored
        std::cout << "\nMAHIMA: DEBUG - Stored values: min_age=" << config.min_age 
                  << ", max_age=" << config.max_age
                  << ", gender=" << (config.target_gender == core::Gender::male ? "male" : 
                                   config.target_gender == core::Gender::female ? "female" : "unknown")
                  << ", risk_factor='" << config.target_risk_factor << "'"
                  << ", year=" << config.target_year
                  << ", scenario='" << config.target_scenario << "'";
    }
}

// MAHIMA: Set debug configuration for single age (convenience method)
void RiskFactorInspector::set_debug_config_single_age(bool enabled, int age, 
                                                     core::Gender gender, const std::string &risk_factor, int target_year,
                                                     const std::string &target_scenario) {
    if (age == -1) {
        // No age restriction
        set_debug_config(enabled, -1, -1, gender, risk_factor, target_year, target_scenario);
    } else {
        // Single age - set both min and max to the same value
        set_debug_config(enabled, age, age, gender, risk_factor, target_year, target_scenario);
    }
}

// MAHIMA: Add multiple debug configurations for different capture scenarios
void RiskFactorInspector::add_debug_configs(const std::vector<DebugConfig> &configs) {
    for (const auto &config : configs) {
        debug_configs_.push_back(config);
    }
    
    std::cout << "\nMAHIMA: Added " << configs.size() << " debug configurations. Total configs: " << debug_configs_.size();
}

// MAHIMA: Add a single debug configuration (convenience method)
void RiskFactorInspector::add_debug_config(bool enabled, int min_age, int max_age, 
                                          core::Gender gender, const std::string &risk_factor, int target_year,
                                          const std::string &target_scenario) {
    DebugConfig config;
    config.enabled = enabled;
    config.min_age = min_age;
    config.max_age = max_age;
    config.target_gender = gender;
    config.target_risk_factor = risk_factor;
    config.target_year = target_year;
    config.target_scenario = target_scenario;
    
    debug_configs_.push_back(config);
}

// MAHIMA: Clear all debug configurations
void RiskFactorInspector::clear_debug_configs() {
    debug_configs_.clear();
    std::cout << "\nMAHIMA: Cleared all debug configurations";
}

// MAHIMA: Check if debug is enabled
bool RiskFactorInspector::is_debug_enabled() const {
    return !debug_configs_.empty() && std::any_of(debug_configs_.begin(), debug_configs_.end(),
                                                   [](const DebugConfig &config) { return config.enabled; });
}

// MAHIMA: Get target risk factor from debug config
const std::string& RiskFactorInspector::get_target_risk_factor() const {
    // Return the first enabled config's risk factor, or empty string if none enabled
    for (const auto &config : debug_configs_) {
        if (config.enabled && !config.target_risk_factor.empty()) {
            return config.target_risk_factor;
        }
    }
    static const std::string empty_string = "";
    return empty_string;
}

// MAHIMA: Get all debug configurations
const std::vector<DebugConfig>& RiskFactorInspector::get_debug_configs() const {
    return debug_configs_;
}

// MAHIMA: Get current scenario name from context
std::string RiskFactorInspector::get_current_scenario_name(RuntimeContext &context) const {
    if (context.scenario().type() == ScenarioType::baseline) {
        return "baseline";
    } else {
        return "intervention";
    }
}

// MAHIMA: Capture detailed risk factor calculation steps for debugging
void RiskFactorInspector::capture_detailed_calculation(RuntimeContext & /*context*/, const Person &person, 
                                                      const std::string &risk_factor_name, size_t /*risk_factor_index*/,
                                                      double random_residual_before_cholesky, double residual_after_cholesky,
                                                      double expected_value, double linear_result, double residual,
                                                      double stddev, double combined, double lambda, double boxcox_result,
                                                      double factor_before_clamp, double range_lower, double range_upper, double first_clamped_factor_value,
    double simulated_mean,
                                                      double factors_mean_delta, double value_after_adjustment_before_second_clamp,
                                                      double final_value_after_second_clamp) {
    
    // Check if debugging is enabled and this person/risk factor matches our criteria
    if (!is_debug_enabled()) {
        return;
    }
    
    // Check if this person and risk factor matches ANY of our debug configurations
    bool should_capture = false;
    
    for (const auto &debug_config : debug_configs_) {
        if (!debug_config.enabled) continue;
        
        bool config_matches = true;
        
        // Check age range filter
        if (!debug_config.is_age_in_range(static_cast<int>(person.age))) {
            config_matches = false;
    }
    
    // Check gender filter
        if (debug_config.target_gender != core::Gender::unknown && person.gender != debug_config.target_gender) {
            config_matches = false;
    }
    
    // Check risk factor filter
        if (!debug_config.target_risk_factor.empty() && 
            !core::case_insensitive::equals(std::string_view(risk_factor_name), std::string_view(debug_config.target_risk_factor))) {
            config_matches = false;
        }
        
        if (config_matches) {
            should_capture = true;
            break; // Found a matching configuration
        }
    }
    
    if (!should_capture) {
        return;
    }
    
    // Create filename using the pre-generated timestamp for consistent naming
    // Single file for both baseline and intervention scenarios
    std::string filename = core::to_lower(risk_factor_name) + "_" + timestamp_str_ + ".csv";
    
    // MAHIMA: Use the output directory that was already determined during initialization
    // This should be the user's home directory where main simulation results are stored
    std::filesystem::path main_output_dir = output_dir_;
    
    std::filesystem::path file_path = main_output_dir / filename;
    
    // MAHIMA: Debug output only for first record to avoid spam
    static bool first_detailed_record_written = false;
    if (!first_detailed_record_written) {
        std::cout << "\nMAHIMA: Writing detailed calculation file: " << file_path.filename().string();
        std::cout << "\nMAHIMA: Full path: " << file_path.string();
        first_detailed_record_written = true;
    }
    
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
             << "first_clamped_factor_value,simulated_mean,factors_mean_delta,value_after_adjustment_before_second_clamp,"
             << "final_value_after_second_clamp,bmi\n";
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
         << first_clamped_factor_value << ","
         << simulated_mean << ","
         << factors_mean_delta << ","
         << value_after_adjustment_before_second_clamp << ","
         << final_value_after_second_clamp << ","
         << get_bmi_value(person) << "\n";
    
    file.close();
}

// MAHIMA: Store calculation details for later capture
void RiskFactorInspector::store_calculation_details(const Person &person, const std::string &risk_factor_name, size_t /*risk_factor_index*/,
                                                   double random_residual_before_cholesky, double residual_after_cholesky,
                                                   double expected_value, double linear_result, double residual,
                                                   double stddev, double combined, double lambda, double boxcox_result,
                                                   double factor_before_clamp, double range_lower, double range_upper,
    double first_clamped_factor_value, double simulated_mean,
                                                   double factors_mean_delta, double value_after_adjustment_before_second_clamp,
                                                   double final_value_after_second_clamp) {
    if (!is_debug_enabled()) {
        return;
    }

    // Check if this person and risk factor should be captured
    bool should_capture = false;
    
    for (const auto &debug_config : debug_configs_) {
        if (!debug_config.enabled) continue;
        
        bool config_matches = true;
        
        if (!debug_config.is_age_in_range(static_cast<int>(person.age))) {
            config_matches = false;
        }
        
        if (debug_config.target_gender != core::Gender::unknown && person.gender != debug_config.target_gender) {
            config_matches = false;
        }
        
        if (!debug_config.target_risk_factor.empty() && risk_factor_name != debug_config.target_risk_factor) {
            config_matches = false;
        }
        
        if (config_matches) {
            should_capture = true;
            break; // Found a matching configuration
        }
    }
    
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
    details.first_clamped_factor_value = first_clamped_factor_value;
    details.simulated_mean = simulated_mean;
    details.factors_mean_delta = factors_mean_delta;
    details.value_after_adjustment_before_second_clamp = value_after_adjustment_before_second_clamp;
    details.final_value_after_second_clamp = final_value_after_second_clamp;
    
    calculation_storage_[person_id][risk_factor_name] = details;
}

// MAHIMA: Capture person risk factors after all calculations are complete
void RiskFactorInspector::capture_person_risk_factors(RuntimeContext &context, const Person &person, 
                                                     const std::string &risk_factor_name, size_t /*risk_factor_index*/) {
    if (!is_debug_enabled()) {
        return;
    }

    // Check if this person and risk factor matches ANY of our debug configurations
    bool should_capture = false;
    std::string current_scenario = get_current_scenario_name(context);
    
    for (const auto &debug_config : debug_configs_) {
        if (!debug_config.enabled) continue;
        
        bool config_matches = true;
        
        // Check age range
        if (!debug_config.is_age_in_range(static_cast<int>(person.age))) {
            config_matches = false;
        }
        
        if (debug_config.target_gender != core::Gender::unknown && person.gender != debug_config.target_gender) {
            config_matches = false;
        }
        
        if (!debug_config.target_risk_factor.empty() && risk_factor_name != debug_config.target_risk_factor) {
            config_matches = false;
        }
        
        // Check year - only capture if we're in the target year
        if (!debug_config.is_year_match(context.time_now())) {
            config_matches = false;
        }
        
        // Check scenario - only capture if we're in the target scenario
        if (!debug_config.is_scenario_match(current_scenario)) {
            config_matches = false;
        }
        
        if (config_matches) {
            should_capture = true;
            break; // Found a matching configuration
        }
    }
    
    if (!should_capture) {
        return;
    }
    
    // Debug output for first few matches
    static int debug_count = 0;
    if (debug_count < 5) {
        std::cout << "\nMAHIMA: Capturing person " << person.id() 
                  << " (age " << person.age << ", " 
                  << (person.gender == core::Gender::male ? "male" : "female") 
                  << ") for risk factor " << risk_factor_name 
                  << " in year " << context.time_now()
                  << ", scenario " << current_scenario;
        debug_count++;
    }

    // Count how many people match our criteria (no console output)
    static int match_count = 0;
    match_count++;

    std::string person_id = std::to_string(person.id());
    
    // Check if we have stored calculation details for this person and risk factor
    if (calculation_storage_.find(person_id) == calculation_storage_.end() ||
        calculation_storage_[person_id].find(risk_factor_name) == calculation_storage_[person_id].end()) {
        // If no stored details, create a minimal record with current values
        std::cout << "\nMAHIMA: No stored calculation details for person " << person_id 
                  << ", risk factor " << risk_factor_name << " - creating minimal record";
        
        // Create minimal calculation details with current values
        CalculationDetails details;
        details.random_residual_before_cholesky = 0.0;
        details.residual_after_cholesky = 0.0;
        details.expected_value = 0.0;
        details.linear_result = 0.0;
        details.residual = 0.0;
        details.stddev = 0.0;
        details.combined = 0.0;
        details.lambda = 0.0;
        details.boxcox_result = 0.0;
        details.factor_before_clamp = 0.0;
        details.range_lower = 0.0;
        details.range_upper = 0.0;
        details.first_clamped_factor_value = 0.0;
        details.simulated_mean = 0.0;
        details.factors_mean_delta = 0.0;
        details.value_after_adjustment_before_second_clamp = 0.0;
        details.final_value_after_second_clamp = 0.0;
        
        calculation_storage_[person_id][risk_factor_name] = details;
    }
    
    const auto& details = calculation_storage_[person_id][risk_factor_name];
    
    // Create the CSV file path using the pre-generated timestamp for consistent naming
    // Single file for both baseline and intervention scenarios
    std::string filename = core::to_lower(risk_factor_name) + "_" + timestamp_str_ + ".csv";
    
    // MAHIMA: Use the output directory that was already determined during initialization
    // This should be the user's home directory where main simulation results are stored
    std::filesystem::path main_output_dir = output_dir_;
    
    // MAHIMA: Ensure the directory exists and is writable
    try {
        // Check if directory exists
        if (!std::filesystem::exists(output_dir_)) {
            std::cout << "\nMAHIMA: Output directory " << output_dir_.string() << " does not exist, creating it...";
            std::filesystem::create_directories(output_dir_);
        }
        
        // Test if we can write to the directory by creating a temporary file
        std::filesystem::path test_file = output_dir_ / "test_write_permissions.tmp";
        std::ofstream test_stream(test_file);
        if (!test_stream.is_open()) {
            throw std::runtime_error("Cannot write to directory");
        }
        test_stream.close();
        std::filesystem::remove(test_file); // Clean up test file
        
        // MAHIMA: Only print permission verification once to avoid spam
        static bool permission_verified = false;
        if (!permission_verified) {
            std::cout << "\nMAHIMA: Successfully verified write access to: " << output_dir_.string();
            permission_verified = true;
        }
        
    } catch (const std::exception& e) {
        // If we can't write to the home directory, this is a serious issue
        std::cerr << "\nMAHIMA: ERROR - Cannot write to home directory " << output_dir_.string() 
                  << " (" << e.what() << ")" << std::endl;
        std::cerr << "\nMAHIMA: This means risk factor inspection files cannot be saved!" << std::endl;
        std::cerr << "\nMAHIMA: Please check directory permissions or contact system administrator." << std::endl;
        return; // Exit without creating files
    }
    
    std::filesystem::path csv_path = main_output_dir / filename;
    
    // MAHIMA: Debug output only for first record to avoid spam
    static bool first_record_written = false;
    if (!first_record_written) {
        std::cout << "\nMAHIMA: Writing risk factor inspection file: " << csv_path.filename().string();
        std::cout << "\nMAHIMA: Full path: " << csv_path.string();
        first_record_written = true;
    }
    
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
        file << "person_id,age,gender,year,scenario,risk_factor,region,ethnicity,physical_activity,income_continuous,income_category,"
             << "random_residual_before_cholesky,residual_after_cholesky,RF_value,expected_value,linear_result,"
             << "residual,stddev,combined,lambda,boxcox_result,factor_before_clamp,range_lower,range_upper,"
             << "first_clamped_factor_value,simulated_mean,factors_mean_delta,value_after_adjustment_before_second_clamp,"
             << "final_value_after_second_clamp,weight_kg,height_cm,bmi_calculated,energy_intake,"
             << "ei_expected,pa_expected,epa_expected,ei_actual,pa_actual,epa_actual,epa_quantile,"
             << "w_expected,w_quantile,initial_weight,weight_adjustment,adjusted_weight,weight_after_clamping,"
             << "body_fat,lean_tissue,glycogen,water,extracellular_fluid\n";
    }
    
    // Write the data in the new order: person_id,age,gender,year,scenario,risk_factor,...
    std::string bmi_value = get_bmi_value(person);
    
    // Get weight and height for BMI calculation elements
    std::string weight_str = get_safe_risk_factor_value(person, "Weight"_id);
    std::string height_str = get_safe_risk_factor_value(person, "Height"_id);
    
    // Get energy intake value
    std::string energy_intake_str = get_safe_risk_factor_value(person, "EnergyIntake"_id);
    
    // Get weight calculation elements from KevinHallModel
    std::string ei_expected_str = get_safe_risk_factor_value(person, "EnergyIntake_expected"_id);
    std::string pa_expected_str = get_safe_risk_factor_value(person, "PhysicalActivity_expected"_id);
    std::string epa_expected_str = get_safe_risk_factor_value(person, "EPA_expected"_id);
    std::string ei_actual_str = get_safe_risk_factor_value(person, "EnergyIntake"_id);
    std::string pa_actual_str = get_safe_risk_factor_value(person, "PhysicalActivity"_id);
    std::string epa_actual_str = get_safe_risk_factor_value(person, "EPA_actual"_id);
    std::string epa_quantile_str = get_safe_risk_factor_value(person, "EPA_quantile"_id);
    std::string w_expected_str = get_safe_risk_factor_value(person, "Weight_expected"_id);
    std::string w_quantile_str = get_safe_risk_factor_value(person, "Weight_quantile"_id);
    std::string initial_weight_str = get_safe_risk_factor_value(person, "Weight_initial"_id);
    std::string weight_adjustment_str = get_safe_risk_factor_value(person, "Weight_adjustment"_id);
    std::string adjusted_weight_str = get_safe_risk_factor_value(person, "Weight_adjusted"_id);
    std::string weight_after_clamping_str = get_safe_risk_factor_value(person, "Weight"_id);
    std::string body_fat_str = get_safe_risk_factor_value(person, "BodyFat"_id);
    std::string lean_tissue_str = get_safe_risk_factor_value(person, "LeanTissue"_id);
    std::string glycogen_str = get_safe_risk_factor_value(person, "Glycogen"_id);
    std::string water_str = get_safe_risk_factor_value(person, "Water"_id);
    std::string extracellular_fluid_str = get_safe_risk_factor_value(person, "ExtracellularFluid"_id);
    
    file << person_id << ","
         << person.age << ","
         << (person.gender == core::Gender::male ? "male" : "female") << ","
         << context.time_now() << ","
         << current_scenario << ","
         << risk_factor_name << ","
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
         << details.first_clamped_factor_value << ","
         << details.simulated_mean << ","
         << details.factors_mean_delta << ","
         << details.value_after_adjustment_before_second_clamp << ","
         << details.final_value_after_second_clamp << ","
         << weight_str << ","
         << height_str << ","
         << bmi_value << ","
         << energy_intake_str << ","
         << ei_expected_str << ","
         << pa_expected_str << ","
         << epa_expected_str << ","
         << ei_actual_str << ","
         << pa_actual_str << ","
         << epa_actual_str << ","
         << epa_quantile_str << ","
         << w_expected_str << ","
         << w_quantile_str << ","
         << initial_weight_str << ","
         << weight_adjustment_str << ","
         << adjusted_weight_str << ","
         << weight_after_clamping_str << ","
         << body_fat_str << ","
         << lean_tissue_str << ","
         << glycogen_str << ","
         << water_str << ","
         << extracellular_fluid_str << "\n";
    
    file.close();
    
    // Count records written and show progress
    total_records_written_++;
    
    // Show progress every 200 records
    if (total_records_written_ % 2000 == 0) {
        std::cout << "\nMAHIMA: Written " << total_records_written_ << " records to " << csv_path.filename().string();
    }
    
    // Show first record message
    if (total_records_written_ == 1) {
        std::cout << "\nMAHIMA: Starting to write records to " << csv_path.filename().string() << "...";
        std::cout << "\nMAHIMA: Writing data for year " << context.time_now() 
                  << ", risk factor " << risk_factor_name
                  << ", scenario " << current_scenario;
        std::cout << "\nMAHIMA: CSV file location: " << csv_path.string();
    }
}

// MAHIMA: Analyze population and count people matching debug criteria
void RiskFactorInspector::analyze_population_demographics(RuntimeContext &context) {
    if (!is_debug_enabled()) {
        return;
    }
    
    int total_population = static_cast<int>(context.population().size());
    
    // Show analysis for each enabled configuration
    for (const auto &debug_config : debug_configs_) {
        if (!debug_config.enabled) continue;
        
    int matching_age_gender = 0;
    int matching_age_gender_risk_factor = 0;
    
    // Count people matching age and gender criteria
    for (const auto &person : context.population()) {
            bool age_matches = debug_config.is_age_in_range(static_cast<int>(person.age));
            bool gender_matches = (debug_config.target_gender == core::Gender::unknown) || (person.gender == debug_config.target_gender);
        
        if (age_matches && gender_matches) {
            matching_age_gender++;
            
            // Check if this person has the target risk factor
                if (!debug_config.target_risk_factor.empty()) {
                    auto risk_factor_id = core::Identifier(debug_config.target_risk_factor);
                if (person.risk_factors.find(risk_factor_id) != person.risk_factors.end()) {
                    matching_age_gender_risk_factor++;
                }
            }
        }
    }
    
    // Show population analysis results
        std::string age_range_str;
        if (debug_config.min_age == -1 && debug_config.max_age == -1) {
            age_range_str = "any age";
        } else if (debug_config.min_age == -1) {
            age_range_str = "age <= " + std::to_string(debug_config.max_age);
        } else if (debug_config.max_age == -1) {
            age_range_str = "age >= " + std::to_string(debug_config.min_age);
        } else {
            age_range_str = "age " + std::to_string(debug_config.min_age) + "-" + std::to_string(debug_config.max_age);
        }
        
    std::cout << "\nMAHIMA: Population analysis - Total: " << total_population 
                  << ", Matching " << age_range_str << " " 
                  << (debug_config.target_gender == core::Gender::male ? "males" : 
                      debug_config.target_gender == core::Gender::female ? "females" : "people")
                  << " for " << debug_config.target_risk_factor << ": " << matching_age_gender_risk_factor << " people";
        
        // Debug: Show what we're actually looking for
        std::cout << "\nMAHIMA: DEBUG - Looking for: age " << debug_config.min_age << "-" << debug_config.max_age
                  << ", gender " << (debug_config.target_gender == core::Gender::male ? "male" : 
                                   debug_config.target_gender == core::Gender::female ? "female" : "any")
                  << ", risk_factor '" << debug_config.target_risk_factor << "'"
                  << ", year " << debug_config.target_year
                  << ", scenario '" << debug_config.target_scenario << "'";
    }
    
    // Show final record count
    if (total_records_written_ > 0) {
        std::cout << "\nMAHIMA: Total records written to inspection CSV files: " << total_records_written_;
    }
}

// MAHIMA: Update stored calculation details with adjustment values
void RiskFactorInspector::update_calculation_details_with_adjustments(const Person &person, const std::string &risk_factor_name,
                                                                     double expected_value, double simulated_mean, double factors_mean_delta,
                                                                     double value_after_adjustment_before_second_clamp,
                                                                     double final_value_after_second_clamp) {
    if (!is_debug_enabled()) {
        return;
    }

    // Check if this person and risk factor should be updated
    bool should_update = false;
    
    for (const auto &debug_config : debug_configs_) {
        if (!debug_config.enabled) continue;
        
        bool config_matches = true;
        
        // Check age range
        if (!debug_config.is_age_in_range(static_cast<int>(person.age))) {
            config_matches = false;
        }
        
        if (debug_config.target_gender != core::Gender::unknown && person.gender != debug_config.target_gender) {
            config_matches = false;
        }
        
        if (!debug_config.target_risk_factor.empty() && risk_factor_name != debug_config.target_risk_factor) {
            config_matches = false;
        }
        
        if (config_matches) {
            should_update = true;
            break; // Found a matching configuration
        }
    }
    
    if (!should_update) {
        return;
    }

    // Update the stored calculation details
    std::string person_id = std::to_string(person.id());
    
    if (calculation_storage_.find(person_id) != calculation_storage_.end() &&
        calculation_storage_[person_id].find(risk_factor_name) != calculation_storage_[person_id].end()) {
        
        auto& details = calculation_storage_[person_id][risk_factor_name];
        details.expected_value = expected_value;  // Store expected value from factors mean table
        details.simulated_mean = simulated_mean;
        details.factors_mean_delta = factors_mean_delta;
        details.value_after_adjustment_before_second_clamp = value_after_adjustment_before_second_clamp;
        details.final_value_after_second_clamp = final_value_after_second_clamp;
    }
}

// MAHIMA: Get stored calculation details for a person and risk factor
bool RiskFactorInspector::get_stored_calculation_details(const Person &person, const std::string &risk_factor_name, CalculationDetails &details) {
    if (!is_debug_enabled()) {
        return false;
    }

    // Check if this person and risk factor should be retrieved
    bool should_retrieve = false;
    
    for (const auto &debug_config : debug_configs_) {
        if (!debug_config.enabled) continue;
        
        bool config_matches = true;
        
        // Check age range
        if (!debug_config.is_age_in_range(static_cast<int>(person.age))) {
            config_matches = false;
        }
        
        if (debug_config.target_gender != core::Gender::unknown && person.gender != debug_config.target_gender) {
            config_matches = false;
        }
        
        if (!debug_config.target_risk_factor.empty() && risk_factor_name != debug_config.target_risk_factor) {
            config_matches = false;
        }
        
        if (config_matches) {
            should_retrieve = true;
            break; // Found a matching configuration
        }
    }
    
    if (!should_retrieve) {
        return false;
    }

    // Get the stored calculation details
    std::string person_id = std::to_string(person.id());
    
    if (calculation_storage_.find(person_id) != calculation_storage_.end() &&
        calculation_storage_[person_id].find(risk_factor_name) != calculation_storage_[person_id].end()) {
        
        details = calculation_storage_[person_id][risk_factor_name];
        return true;
    }
    
    return false;
}

// MAHIMA: Update BMI values in stored calculation details after BMI calculation
void RiskFactorInspector::update_bmi_in_stored_details(const Person &person) {
    if (!is_debug_enabled()) {
        return;
    }

    // Check if this person should be updated based on debug criteria
    bool should_update = false;
    
    for (const auto &debug_config : debug_configs_) {
        if (!debug_config.enabled) continue;
        
        bool config_matches = true;
        
        // Check age range
        if (!debug_config.is_age_in_range(static_cast<int>(person.age))) {
            config_matches = false;
        }
        
        if (debug_config.target_gender != core::Gender::unknown && person.gender != debug_config.target_gender) {
            config_matches = false;
        }
        
        if (config_matches) {
            should_update = true;
            break; // Found a matching configuration
        }
    }
    
    if (!should_update) {
        return;
    }

    // Get the person's ID
    std::string person_id = std::to_string(person.id());
    
    // Check if we have stored calculation details for this person
    if (calculation_storage_.find(person_id) != calculation_storage_.end()) {
        // Get the current BMI value from the person's risk factors
        double current_bmi = 0.0;
        if (person.risk_factors.contains("BMI"_id)) {
            current_bmi = person.risk_factors.at("BMI"_id);
        } else {
            // Calculate BMI on-the-fly if not stored
            if (person.risk_factors.contains("Weight"_id) && person.risk_factors.contains("Height"_id)) {
                double weight = person.risk_factors.at("Weight"_id);
                double height = person.risk_factors.at("Height"_id);
                
                if (height > 0.0 && !std::isnan(weight) && !std::isnan(height) && 
                    !std::isinf(weight) && !std::isinf(height)) {
                    double height_m = height / 100.0;
                    current_bmi = weight / (height_m * height_m);
                }
            }
        }
        
        // Update BMI for all stored risk factors for this person
        // Note: We don't need to store BMI separately since it's calculated from Weight and Height
        // The get_bmi_value() function will use the current BMI value when writing the CSV
        // This method is here for future extensibility if we need to store BMI separately
    }
}


} // namespace hgps
