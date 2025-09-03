#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Input/model_parser.h"
#include "demographic.h"
#include "risk_factor_inspector.h"

#include <algorithm> // Added for std::transform
#include <iomanip>
#include <iostream>
#include <ranges>
#include <utility>
#include <fstream>
#include <cmath> // Added for std::isfinite

namespace hgps {

// MAHIMA: TOGGLE FOR YEAR 3 RISK FACTOR INSPECTION
// Change this to 'true' to enable Year 3 policy inspection data capture
// Change this to 'false' to disable it (default for normal simulations)
static constexpr bool ENABLE_YEAR3_RISK_FACTOR_INSPECTION = false;

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::trace_fat_calculation(const std::string& step_name, 
                                             const Person& person, 
                                             double fat_value,
                                             double expected_value,
                                             double linear_result,
                                             double residual,
                                             double stddev,
                                             double combined,
                                             double lambda,
                                             double boxcox_result,
                                             double factor_before_clamp,
                                             double range_lower,
                                             double range_upper,
                                             double final_clamped_factor,
                                             double random_residual_before_cholesky,
                                             double residual_after_cholesky) const {
    if constexpr (ENABLE_FAT_TRACING) {
        // Only trace for target demographic in year 1 baseline
        if (person.age == TARGET_AGE && 
            person.gender == TARGET_GENDER) {
            
            // Check if file exists to determine if we need to write header
            std::ifstream check_file("fat_calculation_trace.csv");
            bool file_exists = check_file.good();
            check_file.close();
            
            // Write to CSV file (in current working directory)
            std::ofstream csv_file("fat_calculation_trace.csv", std::ios::app);
            
            // Print the file location for debugging
            if (!file_exists) {
                std::cout << "\n" << std::string(80, '=') << std::endl;
                std::cout << "🔍 FAT TRACING DEBUG: Creating fat calculation trace file at: " 
                          << std::filesystem::current_path() / "fat_calculation_trace.csv" << std::endl;
                std::cout << std::string(80, '=') << std::endl;
            }
            if (csv_file.is_open()) {
                // Write header if file is new
                if (!file_exists) {
                    csv_file << "person_id,step_name,fat_value,expected_value,linear_result,residual,stddev,combined,lambda,boxcox_result,factor_before_clamp,range_lower,range_upper,final_clamped_factor,random_residual_before_cholesky,residual_after_cholesky,gender,age,region,ethnicity,physical_activity,income_continuous,income_category\n";
                }
                
                csv_file << person.id() << ","
                         << step_name << ","
                         << fat_value << ","
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
                         << final_clamped_factor << ","
                         << random_residual_before_cholesky << ","
                         << residual_after_cholesky << ","
                         << person.gender_to_value() << ","
                         << person.age << ","
                         << (person.region == core::Region::England ? "England" : 
                             person.region == core::Region::Wales ? "Wales" :
                             person.region == core::Region::Scotland ? "Scotland" :
                             person.region == core::Region::NorthernIreland ? "NorthernIreland" : "Unknown") << ","
                         << (person.ethnicity == core::Ethnicity::White ? "White" :
                             person.ethnicity == core::Ethnicity::Black ? "Black" :
                             person.ethnicity == core::Ethnicity::Asian ? "Asian" :
                             person.ethnicity == core::Ethnicity::Mixed ? "Mixed" :
                             person.ethnicity == core::Ethnicity::Other ? "Other" : "Unknown") << ","
                         << person.get_risk_factor_value("PhysicalActivity") << ","
                         << person.income_continuous << ","
                         << person.income_to_value() << "\n";
                csv_file.close();
            }
        }
    }
}

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    // Verify that all expected risk factors are included in the names_ vector
    verify_risk_factors();

    // NOTE: Demographic variables (region, ethnicity, income, etc.) are already
    // initialized by the DemographicModule in initialise_population

    // Initialise everyone with risk factors.
    for (auto &person : context.population()) {
        initialise_factors(context, person, context.random());
        initialise_physical_activity(context, person, context.random());
        
        // Trace initial BoxCox calculation after both factors and physical activity are set
        if (person.age == TARGET_AGE && person.gender == TARGET_GENDER) {
            double fat_value = person.get_risk_factor_value(TARGET_RISK_FACTOR);
            double expected_value = get_expected(context, person.gender, person.age, TARGET_RISK_FACTOR, ranges_[0], false);
            
            // Get stored calculation parameters if available
            double linear_result = person.get_risk_factor_value(core::Identifier("temp_linear_result"));
            double residual = person.get_risk_factor_value(core::Identifier("temp_residual"));
            double stddev = person.get_risk_factor_value(core::Identifier("temp_stddev"));
            double combined = person.get_risk_factor_value(core::Identifier("temp_combined"));
            double lambda = person.get_risk_factor_value(core::Identifier("temp_lambda"));
            double boxcox_result = person.get_risk_factor_value(core::Identifier("temp_boxcox_result"));
            double factor_before_clamp = person.get_risk_factor_value(core::Identifier("temp_factor_before_clamp"));
            double range_lower = person.get_risk_factor_value(core::Identifier("temp_range_lower"));
            double range_upper = person.get_risk_factor_value(core::Identifier("temp_range_upper"));
            double final_clamped_factor = person.get_risk_factor_value(core::Identifier("temp_final_clamped_factor"));
            double random_residual_before_cholesky = person.get_risk_factor_value(core::Identifier("temp_random_residual_before_cholesky"));
            double residual_after_cholesky = person.get_risk_factor_value(core::Identifier("temp_residual_after_cholesky"));
            
            trace_fat_calculation("initial_boxcox_calculation", person, fat_value, expected_value, 
                                 linear_result, residual, stddev, combined, lambda, 
                                 boxcox_result, factor_before_clamp, range_lower, range_upper, final_clamped_factor, 
                                 random_residual_before_cholesky, residual_after_cholesky);
            
            // Clean up temporary values
            person.risk_factors.erase(core::Identifier("temp_linear_result"));
            person.risk_factors.erase(core::Identifier("temp_residual"));
            person.risk_factors.erase(core::Identifier("temp_stddev"));
            person.risk_factors.erase(core::Identifier("temp_combined"));
            person.risk_factors.erase(core::Identifier("temp_lambda"));
            person.risk_factors.erase(core::Identifier("temp_boxcox_result"));
            person.risk_factors.erase(core::Identifier("temp_factor_before_clamp"));
            person.risk_factors.erase(core::Identifier("temp_range_lower"));
            person.risk_factors.erase(core::Identifier("temp_range_upper"));
            person.risk_factors.erase(core::Identifier("temp_final_clamped_factor"));
            person.risk_factors.erase(core::Identifier("temp_random_residual_before_cholesky"));
            person.risk_factors.erase(core::Identifier("temp_residual_after_cholesky"));
        }
    }

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Trace fat values after first adjustment
    for (auto &person : context.population()) {
        if (person.age == TARGET_AGE && person.gender == TARGET_GENDER) {
            double fat_value = person.get_risk_factor_value(TARGET_RISK_FACTOR);
            double expected_value = get_expected(context, person.gender, person.age, TARGET_RISK_FACTOR, ranges_[0], false);
            trace_fat_calculation("after_1st_adjustment", person, fat_value, expected_value, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        }
    }

    // Initialise everyone with policies and trends.
    for (auto &person : context.population()) {
        initialise_policies(person, context.random(), false);
        initialise_trends(context, person);
    }
    
    // Adjust such that trended risk factor means match trended expected values.
    adjust_risk_factors(context, names_, ranges_, true);
        
    // Trace fat values after second adjustment
    for (auto &person : context.population()) {
        if (person.age == TARGET_AGE && person.gender == TARGET_GENDER) {
            double fat_value = person.get_risk_factor_value(TARGET_RISK_FACTOR);
            double expected_value = get_expected(context, person.gender, person.age, TARGET_RISK_FACTOR, ranges_[0], true);
            trace_fat_calculation("after_2nd_adjustment", person, fat_value, expected_value, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        }
    }

    // Print risk factor summary once at the end
    std::string risk_factor_list;
    for (size_t i = 0; i < names_.size(); i++) {
        if (i > 0)
            risk_factor_list += ", ";
        risk_factor_list += names_[i].to_string();
    }
    
    // Trace final fat values
    for (auto &person : context.population()) {
        if (person.age == TARGET_AGE && person.gender == TARGET_GENDER) {
            double fat_value = person.get_risk_factor_value(TARGET_RISK_FACTOR);
            double expected_value = get_expected(context, person.gender, person.age, TARGET_RISK_FACTOR, ranges_[0], true);
            trace_fat_calculation("final_value", person, fat_value, expected_value, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        }
    }
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {

    // HACK: start intervening two years into the simulation.
    bool intervene = (context.scenario().type() == ScenarioType::intervention &&
                      (context.time_now() - context.start_time()) >= 2);

    // Update risk factors for all people, initializing for newborns.
    // std::cout << "\nDEBUG: StaticLinearModel::update_risk_factors - Beginning to process people";
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            // For newborns, initialize demographic variables
            // initialise_sector(person, context.random());
            //  Demographic variables (region, ethnicity, income) are initialized by the
            //  DemographicModule
            initialise_factors(context, person, context.random());
            initialise_physical_activity(context, person, context.random());
        } else {
            // For existing people, only update sector and factors
            // update_sector(person, context.random());
            update_factors(context, person, context.random());
        }
    }

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Update policies and trends for all people, initializing for newborns.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            initialise_policies(person, context.random(), intervene);
                initialise_trends(context, person);
        } else {
            update_policies(person, intervene);
                update_trends(context, person);
        }
    }

    // Adjust such that trended risk factor means match trended expected values.
        adjust_risk_factors(context, names_, ranges_, true);

    // Apply policies if intervening.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        apply_policies(person, intervene);
    }

    // MAHIMA: Year 3 Risk Factor Data Capture for Policy Inspection
    // This captures individual person risk factor values immediately after policies
    // have been applied to help identify outliers and debug weird/incorrect values
    // that may appear as a result of policy application
    // It is set to FALSE now, if needed, go to line 16 of this file and set it to TRUE (if ypu do
    // only for static linear model is also fine, the others are for detailed print statements)
    // Also set to TRUE in simulation.cpp line 31 and runtime_context.cpp line 9
    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        if (context.has_risk_factor_inspector()) {
            auto &inspector = context.get_risk_factor_inspector();

            // MAHIMA: Check if this is Year 3 and we should capture data
            if (inspector.should_capture_year_3(context)) {
                // MAHIMA: Log the capture attempt for debugging
                std::cout
                    << "\nMAHIMA: StaticLinearModel triggering Year 3 risk factor data capture...";
                std::cout << "\n  Intervention status: " << (intervene ? "Active" : "Inactive");
                std::cout << "\n  Scenario type: "
                          << (context.scenario().type() == ScenarioType::baseline ? "Baseline"
                                                                                  : "Intervention");
                std::cout << "\n  Population size: " << context.population().size();

                // MAHIMA: Trigger the actual data capture
                inspector.capture_year_3_data(context);

                std::cout << "\nMAHIMA: Year 3 data capture completed from StaticLinearModel.";
            } else {
                // MAHIMA: Optional debug message if no inspector is available
                // This helps diagnose setup issues during development
                static bool warning_shown = false;
                if (!warning_shown && (context.time_now() - context.start_time()) == 2) {
                    std::cout << "\nMAHIMA: Note - Risk Factor Inspector for Year 3 data capture "
                                 "is turned off.";
                    warning_shown = true;
                }
            }
        }
    }
}

double StaticLinearModel::inverse_box_cox(double factor, double lambda) {
    // Handle extreme lambda values
    if (std::abs(lambda) < 1e-10) {
        // For lambda near zero, use exponential
        double result = std::exp(factor);
        // Check for Inf
        if (!std::isfinite(result)) {
            return 0.0; // Return safe value for Inf
        }
        return result;
    } else {
        // For non-zero lambda
        double base = (lambda * factor) + 1.0;
        // Check if base is valid for power
        if (base <= 0.0) {
            return 0.0; // Return safe value for negative/zero base
        }

        // Compute power and check result
        double result = std::pow(base, 1.0 / lambda);
        // Validate the result is finite
        if (!std::isfinite(result)) {
            return 0.0; // Return safe value for NaN/Inf
        }

        // Ensure result is non-negative (though power should already guarantee this)
        return std::max(0.0, result);
    }
}

// Calculate the probability of a risk factor being zero using logistic regression- Mahima
double StaticLinearModel::calculate_zero_probability(Person &person,
                                                     size_t risk_factor_index) const {
    // Get the logistic model for this risk factor
    const auto &logistic_model = logistic_models_[risk_factor_index];

    // Calculate the linear predictor using the logistic model
    double logistic_linear_term = logistic_model.intercept;

    // Add the coefficients
    for (const auto &[coef_name, coef_value] : logistic_model.coefficients) {
        logistic_linear_term += coef_value * person.get_risk_factor_value(coef_name);
    }

    // logistic function: p = 1 / (1 + exp(-linear_term))
    double probability = 1.0 / (1.0 + std::exp(-logistic_linear_term));

    // Only print if probability is outside the valid range [0,1] coz logistic regression should be
    // only within 0 and 1 This should never happen with a proper logistic function, but check for
    // numerical issues
    if (probability < 0.0 || probability > 1.0) {
        std::cout << "\nWARNING: Invalid logistic probability for "
                  << names_[risk_factor_index].to_string() << ": " << probability
                  << " (should be between 0 and 1)";
    }
    return probability;
}

void StaticLinearModel::initialise_factors(RuntimeContext &context, Person &person,
                                           Random &random) const {

    // Correlated residual sampling.
    auto [random_residuals_before_cholesky, residuals] = compute_residuals(random, cholesky_, stddev_);

    // Approximate risk factors with linear models.
    auto linear = compute_linear_models(person, models_);

    // Initialise residuals and risk factors (do not exist yet).
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise residual.
        auto residual_name = core::Identifier{names_[i].to_string() + "_residual"};
        double residual = residuals[i];

        // Save residual.
        person.risk_factors[residual_name] = residual;

        // Initialise risk factor.
        double expected =
            get_expected(context, person.gender, person.age, names_[i], ranges_[i], false);

        // TWO-STAGE RISK FACTOR MODELING APPROACH- Mahima
        // =======================================================================
        // Previous approach: Use BoxCox transformation to calculate risk factor values
        // New approach: First use logistic regression to determine if the risk factor value should
        //              be zero
        //              If non-zero, then use BoxCox transformation to calculate the actual value
        // =======================================================================

        // MAHIMA: Check if this risk factor has logistic coefficients
        // Empty logistic model means intentionally skip Stage 1 and use boxcox-only modeling
        bool has_logistic_model = !(logistic_models_[i].coefficients.empty());

        // STAGE 1: Determine if risk factor should be zero (only if logistic model exists)
        if (has_logistic_model) {
            double zero_probability = calculate_zero_probability(person, i);

            // Sample from this probability to determine if risk factor should be zero
            // if logistic regression output = 1, risk factor value = 0
            double random_sample = random.next_double(); // Uniform random value between 0 and 1
            if (random_sample < zero_probability) {
                // Risk factor should be zero
                person.risk_factors[names_[i]] = 0.0;
                continue;
            }
        }

        // STAGE 2: Calculate non-zero risk factor value using BoxCox transformation
        // (This code runs whether we have a logistic model or not)
        double factor = linear[i] + residual;
        double combined = factor;
        double boxcox_result = inverse_box_cox(factor, lambda_[i]);
        double factor_before_clamp = expected * boxcox_result;
        factor = ranges_[i].clamp(factor_before_clamp);

        // Save risk factor
        person.risk_factors[names_[i]] = factor;
        
        // Store calculation parameters for tracing after physical activity is set
        if (names_[i] == TARGET_RISK_FACTOR) {
            // Store the calculation parameters for later tracing
            person.risk_factors[core::Identifier("temp_linear_result")] = linear[i];
            person.risk_factors[core::Identifier("temp_residual")] = residual;
            person.risk_factors[core::Identifier("temp_stddev")] = stddev_[i];
            person.risk_factors[core::Identifier("temp_combined")] = combined;
            person.risk_factors[core::Identifier("temp_lambda")] = lambda_[i];
            person.risk_factors[core::Identifier("temp_boxcox_result")] = boxcox_result;
            person.risk_factors[core::Identifier("temp_factor_before_clamp")] = factor_before_clamp;
            person.risk_factors[core::Identifier("temp_range_lower")] = ranges_[i].lower();
            person.risk_factors[core::Identifier("temp_range_upper")] = ranges_[i].upper();
            person.risk_factors[core::Identifier("temp_final_clamped_factor")] = factor;
            person.risk_factors[core::Identifier("temp_random_residual_before_cholesky")] = random_residuals_before_cholesky[i];
            person.risk_factors[core::Identifier("temp_residual_after_cholesky")] = residual;
        }
    }
}

void StaticLinearModel::update_factors(RuntimeContext &context, Person &person,
                                       Random &random) const {

    // Correlated residual sampling.
    auto [new_random_residuals_before_cholesky, new_residuals] = compute_residuals(random, cholesky_, stddev_);

    // Approximate risk factors with linear models.
    auto linear = compute_linear_models(person, models_);

    // Update residuals and risk factors (should exist).
    for (size_t i = 0; i < names_.size(); i++) {

        // Get the risk factor name and expected value
        double expected =
            get_expected(context, person.gender, person.age, names_[i], ranges_[i], false);

        // Blend old and new residuals
        // Update residual.
        auto residual_name = core::Identifier{names_[i].to_string() + "_residual"};
        double residual_old = person.risk_factors.at(residual_name);
        double residual = new_residuals[i] * info_speed_;
        residual += sqrt(1.0 - info_speed_ * info_speed_) * residual_old;

        // Save residual.
        person.risk_factors.at(residual_name) = residual;

        // TWO-STAGE RISK FACTOR MODELING APPROACH- Mahima
        // =======================================================================
        // Use both this year's zero probability and previous outcome
        // =======================================================================

        // MAHIMA: Check if this risk factor has logistic coefficients
        // Empty logistic model means intentionally skip Stage 1 and use boxcox-only modeling
        bool has_logistic_model = !(logistic_models_[i].coefficients.empty());

        // STAGE 1: Determine if risk factor should be zero (only if logistic model exists)
        if (has_logistic_model) {
            double zero_probability = calculate_zero_probability(person, i);

            // HACK: To maintain longitudinal correlation among people, amend their "probability of
            // being a zero" according to their current zero-probability...
            // ... and either 1 if they were a zero, or 0 if they were not.
            if (person.risk_factors[names_[i]] == 0) {
                zero_probability = (zero_probability + 1.0) / 2.0;
            } else {
                zero_probability = (zero_probability + 0.0) / 2.0;
            }

            // Draw random number to determine if risk factor should be zero
            if (random.next_double() < zero_probability) {
                // Risk factor should be zero
                person.risk_factors[names_[i]] = 0.0;
                continue;
            }
        }

        // STAGE 2: Calculate non-zero risk factor value using BoxCox transformation
        // (This code runs whether we have a logistic model or not)
        double factor = linear[i] + residual;
        factor = expected * inverse_box_cox(factor, lambda_[i]);
        factor = ranges_[i].clamp(factor);

        // Save risk factor
        person.risk_factors[names_[i]] = factor;
    }
}

void StaticLinearModel::initialise_trends(RuntimeContext &context, Person &person) const {

    // Approximate trends with linear models.
    auto linear = compute_linear_models(person, *trend_models_);

    // Initialise and apply trends (do not exist yet).
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_trend"};
        double expected = expected_trend_boxcox_->at(names_[i]);
        double trend = expected * inverse_box_cox(linear[i], (*trend_lambda_)[i]);
        trend = (*trend_ranges_)[i].clamp(trend);

        // Save trend.
        person.risk_factors[trend_name] = trend;
    }

    // Apply trends.
    update_trends(context, person);
}

void StaticLinearModel::update_trends(RuntimeContext &context, Person &person) const {

    // Get elapsed time (years).
    int elapsed_time = context.time_now() - context.start_time();

    // Apply trends (should exist).
    for (size_t i = 0; i < names_.size(); i++) {

        // Load trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_trend"};
        double trend = person.risk_factors.at(trend_name);

        // Apply trend to risk factor.
        double factor = person.risk_factors.at(names_[i]);
        int t = std::min(elapsed_time, get_trend_steps(names_[i]));
        factor *= pow(trend, t);
        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors.at(names_[i]) = factor;
    }
}

void StaticLinearModel::initialise_policies(Person &person, Random &random, bool intervene) const {
    // NOTE: we need to keep baseline and intervention scenario RNGs in sync,
    //       so we compute residuals even though they are not used in baseline.

    // Intervention policy residual components.
    auto [policy_random_residuals_before_cholesky, residuals] = compute_residuals(random, policy_cholesky_, stddev_);

    // Save residuals (never updated in lifetime).
    for (size_t i = 0; i < names_.size(); i++) {
        auto residual_name = core::Identifier{names_[i].to_string() + "_policy_residual"};
        person.risk_factors[residual_name] = residuals[i];

        // Ensure the risk factor exists (might be zero from two-stage model)
        if (person.risk_factors.find(names_[i]) == person.risk_factors.end()) {
            std::cout << "\nDEBUG: Risk factor " << names_[i].to_string()
                      << " not found, initializing to zero";
            person.risk_factors[names_[i]] = 0.0;
        }
    }

    // Compute policies.
    update_policies(person, intervene);
}

void StaticLinearModel::update_policies(Person &person, bool intervene) const {

    // Set zero policy if not intervening.
    if (!intervene) {
        for (const auto &name : names_) {
            auto policy_name = core::Identifier{name.to_string() + "_policy"};
            person.risk_factors[policy_name] = 0.0;
        }
        return;
    }

    // Intervention policy linear components.
    auto linear = compute_linear_models(person, policy_models_);

    // Compute all intervention policies.
    for (size_t i = 0; i < names_.size(); i++) {

        // Load residual component.
        auto residual_name = core::Identifier{names_[i].to_string() + "_policy_residual"};
        double residual = person.risk_factors.at(residual_name);

        // Compute policy.
        auto policy_name = core::Identifier{names_[i].to_string() + "_policy"};
        double policy = linear[i] + residual;
        policy = policy_ranges_[i].clamp(policy);

        // Save policy.
        person.risk_factors[policy_name] = policy;
    }
}

void StaticLinearModel::apply_policies(Person &person, bool intervene) const {
    // No-op if not intervening.
    if (!intervene) {
        return;
    }

    // Apply all intervention policies.
    for (size_t i = 0; i < names_.size(); i++) {

        // Load policy.
        auto policy_name = core::Identifier{names_[i].to_string() + "_policy"};
        double policy = person.risk_factors.at(policy_name);

        // Apply policy to risk factor.
        double factor_old = person.risk_factors.at(names_[i]);
        double factor = factor_old * (1.0 + policy / 100.0);
        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors.at(names_[i]) = factor;
    }
}

std::vector<double>
StaticLinearModel::compute_linear_models(Person &person,
                                         const std::vector<LinearModelParams> &models) const {
    std::vector<double> linear{};
    linear.reserve(names_.size());

    // Pre-calculate capped age values once (outside the model loop)- Mahima
    constexpr double max_age = 80.0; // Define appropriate maximum age
    const double capped_age = std::min(static_cast<double>(person.age), max_age);
    const double capped_age_squared = capped_age * capped_age;
    const double capped_age_cubed = capped_age_squared * capped_age;

    // Cache common age identifiers for faster comparison using case sensitive approach for Age and
    // age- Mahima
    static const core::Identifier age_id("age");
    static const core::Identifier age2_id("age2");
    static const core::Identifier age3_id("age3");
    static const core::Identifier Age_id("Age");
    static const core::Identifier Age2_id("Age2");
    static const core::Identifier Age3_id("Age3");
    static const core::Identifier stddev_id("stddev");

    // Approximate risk factors with linear models
    for (size_t i = 0; i < names_.size(); i++) {
        if (i >= models.size()) {
            std::cout << "\nERROR: compute_linear_models - Index " << i
                      << " is out of bounds for models vector of size " << models.size();
            linear.push_back(0.0); // Default value
            continue;
        }

        auto name = names_[i];
        auto model = models[i];
        double factor = model.intercept;

        for (const auto &[coefficient_name, coefficient_value] : model.coefficients) {
            // Skip the standard deviation entry as it's not a factor
            if (coefficient_name == stddev_id)
                continue;

            // Efficiently handle age-related coefficients
            if (coefficient_name == age_id || coefficient_name == Age_id) {
                factor += coefficient_value * capped_age;
            } else if (coefficient_name == age2_id || coefficient_name == Age2_id) {
                factor += coefficient_value * capped_age_squared;
            } else if (coefficient_name == age3_id || coefficient_name == Age3_id) {
                factor += coefficient_value * capped_age_cubed;
            } else {
                // Regular coefficient processing
                double value = person.get_risk_factor_value(coefficient_name);
                factor += coefficient_value * value;
            }
        }

        for (const auto &[coefficient_name, coefficient_value] : model.log_coefficients) {
            double value = person.get_risk_factor_value(coefficient_name);

            if (value <= 0) {
                value = 1e-10; // Avoid log of zero or negative
            }

            // MAHIMA: Verification print for EnergyIntake log coefficient fix
            /* if (coefficient_name == "EnergyIntake"_id) {
                static int print_count = 0;
                if (print_count < 3) { // Only print first 3 times to avoid spam
                    std::cout << "\nMAHIMA VERIFICATION: EnergyIntake log coefficient applied
            correctly!"
                              << "\n  Coefficient value: " << coefficient_value
                              << "\n  EnergyIntake value: " << value
                              << "\n  log(EnergyIntake): " << log(value)
                              << "\n  Linear term contribution: " << coefficient_value * log(value)
                              << "\n  (Previous bug would have been: " << coefficient_value * value
            << ")"; print_count++;
                }
            }*/

            factor += coefficient_value * log(value);
        }

        linear.emplace_back(factor);
    }

    return linear;
}

std::pair<std::vector<double>, std::vector<double>> StaticLinearModel::compute_residuals(Random &random,
                                                                                         const Eigen::MatrixXd &cholesky,
                                                                                         const std::vector<double> &stddev) const {
    std::vector<double> random_residuals_before_cholesky{};
    std::vector<double> correlated_residuals{};
    random_residuals_before_cholesky.reserve(names_.size());
    correlated_residuals.reserve(names_.size());

    // Correlated samples using Cholesky decomposition.
    Eigen::VectorXd residuals{names_.size()};
    
    // Generate residuals with mean=0 and std=stddev[i] for each risk factor
    for (size_t i = 0; i < names_.size(); i++) {
        double random_residual = random.next_normal(0.0, stddev[i]);
        residuals[i] = random_residual;
        random_residuals_before_cholesky.emplace_back(random_residual);
    }
    
    // DEBUG: Print FoodFat residuals for first 5 calls only
    static int debug_call_count = 0;
    
    // Find FoodFat index (case-insensitive search)
    size_t foodfat_index = 0;
    bool foodfat_found = false;
    for (size_t idx = 0; idx < names_.size(); idx++) {
        std::string name_lower = names_[idx].to_string();
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        if (name_lower == "foodfat") {
            foodfat_index = idx;
            foodfat_found = true;
            break;
        }
    }
    
    // DEBUG: Verify that StaticLinear::names_ order matches CSV order
    if (debug_call_count < 3) {
        std::cout << "\n" << std::string(80, '=');
        std::cout << "\nVERIFYING RESIDUAL-CHOLESKY MULTIPLICATION ORDER";
        std::cout << "\n" << std::string(80, '=');
        std::cout << "\nStaticLinear::names_ order (used for residuals):";
        for (size_t idx = 0; idx < names_.size(); idx++) {
            if (idx % 5 == 0) std::cout << "\n  ";
            std::cout << "[" << idx << "]" << names_[idx].to_string() << " ";
        }
        std::cout << "\n";
        std::cout << "\nIMPORTANT: Residuals are in StaticLinear::names_ order, but Cholesky is in CSV order!";
        std::cout << "\nThis means residual[i] is multiplied by cholesky(csv_row_for_names_[i], j)";
        std::cout << "\n" << std::string(80, '=');
    }
    
    if (debug_call_count < 5) {
        if (foodfat_found) {
            std::cout << "\n" << std::string(60, '-');
            std::cout << "\nDEBUG: CHOLESKY MULTIPLICATION FOR FOODFAT (Call #" << (debug_call_count + 1) << ")";
            std::cout << "\n" << std::string(60, '-');
            std::cout << "\nFoodFat index: " << foodfat_index;
            std::cout << "\nFoodFat stddev: " << stddev[foodfat_index];
            std::cout << "\nRandom residual before Cholesky: " << std::fixed << std::setprecision(6) << random_residuals_before_cholesky[foodfat_index];
            
            // Show the Cholesky row for FoodFat with risk factor names
            std::cout << "\nCholesky row for FoodFat (with risk factor names):";
            for (size_t j = 0; j < cholesky.cols() && j < names_.size(); j++) {
                if (j % 3 == 0) std::cout << "\n  ";
                std::cout << std::fixed << std::setprecision(3) << cholesky(foodfat_index, j) 
                         << "(" << names_[j].to_string() << ") ";
            }
            
            // Show which Cholesky values are contributing most to the final result
            std::cout << "\n\nCholesky contributions (value * residual):";
            double total_contribution = 0.0;
            for (size_t j = 0; j < cholesky.cols() && j < names_.size(); j++) {
                double contribution = cholesky(foodfat_index, j) * random_residuals_before_cholesky[j];
                total_contribution += contribution;
                if (std::abs(contribution) > 0.1) { // Only show significant contributions
                    std::cout << "\n  " << names_[j].to_string() << ": " 
                             << std::fixed << std::setprecision(3) << cholesky(foodfat_index, j) 
                             << " * " << std::fixed << std::setprecision(6) << random_residuals_before_cholesky[j]
                             << " = " << std::fixed << std::setprecision(6) << contribution;
                }
            }
            std::cout << "\n  Total contribution: " << std::fixed << std::setprecision(6) << total_contribution;
        }
        debug_call_count++;
    }
    
    residuals = cholesky * residuals;

    // DEBUG: Print FoodFat correlated residual for first 5 calls only
    if (debug_call_count <= 5 && foodfat_found) {
        std::cout << "\nCorrelated residual after Cholesky: " << std::fixed << std::setprecision(6) << residuals[foodfat_index];
        std::cout << "\nAmplification factor: " << std::fixed << std::setprecision(2) 
                  << (std::abs(residuals[foodfat_index]) / std::abs(random_residuals_before_cholesky[foodfat_index]));
        std::cout << "\n" << std::string(60, '-') << "\n";
    }

    // Save correlated residuals.
    for (size_t i = 0; i < names_.size(); i++) {
        correlated_residuals.emplace_back(residuals[i]);
    }

    return {random_residuals_before_cholesky, correlated_residuals};
}

void StaticLinearModel::initialise_sector(Person &person, Random &random) const {

    // Get rural prevalence for age group and sex.
    double prevalence;
    if (person.age < 18) {
        prevalence = rural_prevalence_.at("Under18"_id).at(person.gender);
    } else {
        prevalence = rural_prevalence_.at("Over18"_id).at(person.gender);
    }

    // Sample the person's sector.
    double rand = random.next_double();
    auto sector = rand < prevalence ? core::Sector::rural : core::Sector::urban;
    person.sector = sector;
}

void StaticLinearModel::update_sector(Person &person, Random &random) const {

    // Only update rural sector 18 year olds.
    if ((person.age != 18) || (person.sector != core::Sector::rural)) {
        return;
    }

    // Get rural prevalence for age group and sex.
    double prevalence_under18 = rural_prevalence_.at("Under18"_id).at(person.gender);
    double prevalence_over18 = rural_prevalence_.at("Over18"_id).at(person.gender);

    // Compute random rural to urban transition.
    double rand = random.next_double();
    double p_rural_to_urban = 1.0 - prevalence_over18 / prevalence_under18;
    if (rand < p_rural_to_urban) {
        person.sector = core::Sector::urban;
    }
}
// Physical activity depends on age. gender, region, ethnicity, income_continuous and random noise
// with std dev Loaded from the static_model.json under the Physical Activity Models section
// NOLINTBEGIN(readability-function-cognitive-complexity)
void StaticLinearModel::initialise_physical_activity([[maybe_unused]] RuntimeContext &context,
                                                     Person &person, Random &random) const {
    // std::cout << "\nDEBUG: Inside physical activity initialization";

    // Check if we have any models before proceeding
    if (physical_activity_models_.empty()) {
        std::cout
            << "\nERROR: physical_activity_models_ is empty! Cannot initialize physical activity.";
        std::cout << "\nERROR: Please check that PhysicalActivityModels are properly defined in "
                     "the configuration.";
        // Don't set a default value - let the error surface so it can be diagnosed
        return;
    }

    // Continue only if models are available
    double final_value = 0.0;

    // Look for the continuous model first
    auto model_it = physical_activity_models_.find(core::Identifier("continuous"));

    // If continuous model not found, use the first available model
    if (model_it == physical_activity_models_.end()) {
        std::cout << "\nERROR: No 'continuous' model found, using first available model";
        model_it = physical_activity_models_.begin();
    }

    // Check again that we have a valid iterator
    if (model_it != physical_activity_models_.end()) {
        const auto &model = model_it->second;

        // Start with the intercept
        double value = model.intercept;
        // std::cout << "\nDEBUG: Using physical activity intercept value: " << value;

        // Process coefficients
        for (const auto &[factor_name, coefficient] : model.coefficients) {
            // Skip the standard deviation entry as it's not a factor
            if (factor_name == "stddev"_id) {
                continue;
            }
            // Apply coefficient based on its name
            double factor_value = 0.0;

            // Age effects
            if (factor_name == "age"_id) {
                factor_value = static_cast<double>(person.age);
            } else if (factor_name == "age2"_id) {
                factor_value = std::pow(person.age, 2);
            } else if (factor_name == "age3"_id) {
                factor_value = std::pow(person.age, 3);
            }
            // Gender effect
            else if (factor_name == "gender"_id) {
                factor_value = person.gender_to_value();
            }
            // Sector effect
            else if (factor_name == "sector"_id) {
                factor_value = person.sector_to_value();
            }
            // Region effects
            else if (factor_name == "england"_id && person.region == core::Region::England) {
                factor_value = 1.0;
            } else if (factor_name == "wales"_id && person.region == core::Region::Wales) {
                factor_value = 1.0;
            } else if (factor_name == "scotland"_id && person.region == core::Region::Scotland) {
                factor_value = 1.0;
            } else if (factor_name == "northernireland"_id &&
                       person.region == core::Region::NorthernIreland) {
                factor_value = 1.0;
            }
            // Ethnicity effects
            else if (factor_name == "white"_id && person.ethnicity == core::Ethnicity::White) {
                factor_value = 1.0;
            } else if (factor_name == "black"_id && person.ethnicity == core::Ethnicity::Black) {
                factor_value = 1.0;
            } else if (factor_name == "asian"_id && person.ethnicity == core::Ethnicity::Asian) {
                factor_value = 1.0;
            } else if (factor_name == "mixed"_id && person.ethnicity == core::Ethnicity::Mixed) {
                factor_value = 1.0;
            } else if ((factor_name == "others"_id || factor_name == "other"_id) &&
                       person.ethnicity == core::Ethnicity::Other) {
                factor_value = 1.0;
            }
            // Income continuous value
            else if (factor_name == "income"_id) {
                factor_value = person.income_continuous;
            }
            // If we already have this factor, use its value
            else if (person.risk_factors.contains(factor_name)) {
                factor_value = person.risk_factors.at(factor_name);
            }

            // Add to our value
            value += coefficient * factor_value;
        }

        // Get the standard deviation
        double pa_stddev = physical_activity_stddev_;
        if (model.coefficients.contains("stddev"_id)) {
            pa_stddev = model.coefficients.at("stddev"_id);
        }

        // Add random noise using the standard deviation
        double noise = random.next_normal(0.0, pa_stddev);

        // Calculate final value with noise
        final_value = value + noise; // Add noise instead of multiplying

        // Apply min/max bounds if they exist in the model
        final_value = std::max(final_value, model.coefficients.at("min"));
        final_value = std::min(final_value, model.coefficients.at("max"));

        // Set the physical activity value
        person.risk_factors[core::Identifier("PhysicalActivity")] = final_value;
        person.physical_activity = final_value;
    } else {
        std::cout << "\nERROR: No valid physical activity models found!";
        // Don't set a default value - let the error surface
        return;
    }

    // std::cout << "\nDEBUG: Finished physical activity initialization for person ID " <<
    // person.id();
} // NOLINTEND(readability-function-cognitive-complexity)

StaticLinearModelDefinition::StaticLinearModelDefinition(
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox,
    std::vector<core::Identifier> names, std::vector<LinearModelParams> models,
    std::vector<core::DoubleInterval> ranges, std::vector<double> lambda,
    std::vector<double> stddev, Eigen::MatrixXd cholesky,
    std::vector<LinearModelParams> policy_models, std::vector<core::DoubleInterval> policy_ranges,
    Eigen::MatrixXd policy_cholesky, std::unique_ptr<std::vector<LinearModelParams>> trend_models,
    std::unique_ptr<std::vector<core::DoubleInterval>> trend_ranges,
    std::unique_ptr<std::vector<double>> trend_lambda, double info_speed,
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>> rural_prevalence,
    std::unordered_map<core::Identifier,
                       std::unordered_map<core::Gender, std::unordered_map<core::Region, double>>>
        region_prevalence,
    std::unordered_map<
        core::Identifier,
        std::unordered_map<core::Gender, std::unordered_map<core::Ethnicity, double>>>
        ethnicity_prevalence,
    std::unordered_map<core::Income, LinearModelParams> income_models,
    std::unordered_map<core::Region, LinearModelParams> region_models,
    double physical_activity_stddev,
    const std::unordered_map<core::Identifier, LinearModelParams> &physical_activity_models,
    std::vector<LinearModelParams> logistic_models)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps)},
      expected_trend_boxcox_{std::move(expected_trend_boxcox)}, names_{std::move(names)},
      models_{std::move(models)}, ranges_{std::move(ranges)}, lambda_{std::move(lambda)},
      stddev_{std::move(stddev)}, cholesky_{std::move(cholesky)},
      policy_models_{std::move(policy_models)}, policy_ranges_{std::move(policy_ranges)},
      policy_cholesky_{std::move(policy_cholesky)}, trend_models_{std::move(trend_models)},
      trend_ranges_{std::move(trend_ranges)}, trend_lambda_{std::move(trend_lambda)},
      info_speed_{info_speed}, rural_prevalence_{std::move(rural_prevalence)},
      region_prevalence_{std::move(region_prevalence)},
      ethnicity_prevalence_{std::move(ethnicity_prevalence)},
      income_models_{std::move(income_models)}, region_models_{std::move(region_models)},
      physical_activity_stddev_{physical_activity_stddev},
      physical_activity_models_{physical_activity_models},
      logistic_models_{std::move(logistic_models)} {

    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (ranges_.empty()) {
        throw core::HgpsException("Risk factor ranges list is empty");
    }

    // Print the loaded ranges for verification
    std::cout << "\n======= LOADED RISK FACTOR RANGES IN STATIC LINEAR MODEL =======";
    for (size_t i = 0; i < names_.size() && i < ranges_.size(); ++i) {
        std::cout << "\nRisk factor: " << names_[i].to_string() << ", Range: ["
                  << ranges_[i].lower() << " , " << ranges_[i].upper() << "]";
    }
    std::cout << "\n=========================================\n";

    if (lambda_.empty()) {
        throw core::HgpsException("Risk factor lambda list is empty");
    }
    if (stddev_.empty()) {
        throw core::HgpsException("Risk factor standard deviation list is empty");
    }
    if (!cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    if (policy_models_.empty()) {
        throw core::HgpsException("Intervention policy model list is empty");
    }
    if (policy_ranges_.empty()) {
        throw core::HgpsException("Intervention policy ranges list is empty");
    }
    if (!policy_cholesky_.allFinite()) {
        throw core::HgpsException("Intervention policy Cholesky matrix contains non-finite values");
    }
        if (trend_models_->empty()) {
            throw core::HgpsException("Time trend model list is empty");
        }
        if (trend_ranges_->empty()) {
            throw core::HgpsException("Time trend ranges list is empty");
        }
        if (trend_lambda_->empty()) {
            throw core::HgpsException("Time trend lambda list is empty");
        }
    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
        for (const auto &name : names_) {
            if (!expected_trend_->contains(name)) {
                throw core::HgpsException("One or more expected trend value is missing");
            }
            if (!expected_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected trend BoxCox value is missing");
            }
        }

    // If logistic models are empty, initialize them to be the same as the boxcox models
    // This is just a fallback in case logistic models were not provided
    if (logistic_models_.empty()) {
        std::cout
            << "\nWARNING: No logistic regression models provided, using BoxCox models as fallback";
        logistic_models_ = models_;
    }

    // Check if the number of logistic models matches the number of risk factors
    if (logistic_models_.size() != names_.size()) {
                throw core::HgpsException(
            fmt::format("Number of logistic models ({}) does not match number of risk factors ({})",
                        logistic_models_.size(), names_.size()));
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    // Debug information about physical_activity_models_
    // std::cout << "\nDEBUG: In create_model - physical_activity_models_ size: " <<
    // physical_activity_models_.size() << std::endl;

    /*if (!physical_activity_models_.empty()) {
        std::cout << "\nDEBUG: First model key: "
                  << physical_activity_models_.begin()->first.to_string() << std::endl;
    }*/

    return std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_, names_, models_, ranges_,
        lambda_, stddev_, cholesky_, policy_models_, policy_ranges_, policy_cholesky_,
        trend_models_, trend_ranges_, trend_lambda_, info_speed_, rural_prevalence_,
        region_prevalence_, ethnicity_prevalence_, income_models_, region_models_,
        physical_activity_stddev_, get_physical_activity_models(), logistic_models_);
}

// Add a new method to verify risk factors
void StaticLinearModel::verify_risk_factors() const {
    // std::cout << "\nDEBUG: StaticLinearModel::verify_risk_factors - Verifying risk factor
    // configuration";

    // Check model configuration consistency - no hardcoded values
    if (names_.size() != models_.size()) {
        std::cout << "\nWARNING: Mismatch between names_ size (" << names_.size()
                  << ") and models_ size (" << models_.size() << ")";
    }

    if (names_.size() != ranges_.size()) {
        std::cout << "\nWARNING: Mismatch between names_ size (" << names_.size()
                  << ") and ranges_ size (" << ranges_.size() << ")";
    }

    if (names_.size() != lambda_.size()) {
        std::cout << "\nWARNING: Mismatch between names_ size (" << names_.size()
                  << ") and lambda_ size (" << lambda_.size() << ")";
    }

    if (names_.size() != stddev_.size()) {
        std::cout << "\nWARNING: Mismatch between names_ size (" << names_.size()
                  << ") and stddev_ size (" << stddev_.size() << ")";
    }

    // Check RiskFactorModels from expected_trend_boxcox_ map against names_
    if (expected_trend_boxcox_) {
        for (const auto &name : names_) {
            if (!expected_trend_boxcox_->contains(name)) {
                std::cout << "\nWARNING: Risk factor " << name.to_string()
                          << " not found in expected_trend_boxcox_ map";
            }
        }
    }

    // std::cout << "\nDEBUG: StaticLinearModel::verify_risk_factors - Verification completed with "
    // << names_.size() << " risk factors";
}

} // namespace hgps
