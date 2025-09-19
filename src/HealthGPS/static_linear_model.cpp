#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Input/model_parser.h"
#include "demographic.h"
#include "risk_factor_inspector.h"
#include "risk_factor_adjustable_model.h"

#include <iostream>
#include <ranges>
#include <utility>

namespace hgps {

// MAHIMA: TOGGLE FOR YEAR 3 RISK FACTOR INSPECTION
// Change this to 'true' to enable Year 3 policy inspection data capture
// Change this to 'false' to disable it (default for normal simulations)
static constexpr bool ENABLE_YEAR3_RISK_FACTOR_INSPECTION = false;

// MAHIMA: TOGGLE FOR DETAILED CALCULATION DEBUGGING
// Change this to 'true' to enable detailed risk factor calculation debugging
// Change this to 'false' to disable it (default for normal simulations)
static constexpr bool ENABLE_DETAILED_CALCULATION_DEBUG = true;

// Helper function to combine food and other risk factors into a single vector
// This ensures we have one adjustments map with all factors instead of separate maps
std::vector<core::Identifier> StaticLinearModel::combine_risk_factors() const {
    std::vector<core::Identifier> combined_factors;
    
    // Reserve space for food factors + only income and physicalactivity
    combined_factors.reserve(names_.size() + 2);
    
    // Add food factors first (maintains correlation matrix order)
    combined_factors.insert(combined_factors.end(), names_.begin(), names_.end());
    
    // Add only income and physicalactivity from other risk factors
    // height, weight, energyintake are handled by Kevin Hall model
    for (const auto& factor : other_risk_factor_names_) {
        if (factor.to_string() == "income" || factor.to_string() == "physicalactivity") {
            combined_factors.push_back(factor);
        }
    }
    
    return combined_factors;
}

// Helper function to combine food and other risk factor ranges into a single vector
// This ensures each factor has its appropriate range constraints
std::vector<core::DoubleInterval> StaticLinearModel::combine_risk_factor_ranges() const {
    std::vector<core::DoubleInterval> combined_ranges;
    
    // Reserve space for food factor ranges + only income and physicalactivity ranges
    combined_ranges.reserve(ranges_.size() + 2);
    
    // Add food factor ranges first (maintains correlation matrix order)
    combined_ranges.insert(combined_ranges.end(), ranges_.begin(), ranges_.end());
    
    // Add only income and physicalactivity ranges from other risk factors
    // height, weight, energyintake are handled by Kevin Hall model
    for (size_t i = 0; i < other_risk_factor_names_.size(); i++) {
        const auto& factor = other_risk_factor_names_[i];
        if (factor.to_string() == "income" || factor.to_string() == "physicalactivity") {
            combined_ranges.push_back(other_risk_factor_ranges_[i]);
        }
    }
    
    return combined_ranges;
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {
    // Verify that all expected risk factors are included in the names_ vector
    verify_risk_factors();

    // MAHIMA: Initialize logistic factors for simulated mean calculation
    initialize_logistic_factors();

    // NOTE: Demographic variables (region, ethnicity, income, etc.) are already
    // initialized by the DemographicModule in initialise_population

    // Initialise everyone with risk factors.
    for (auto &person : context.population()) {
        initialise_factors(context, person, context.random());
        initialise_physical_activity(context, person, context.random());
        
            // MAHIMA: No debugging capture here - will be done after adjustments
            // Individual-level debugging capture happens after all adjustments are applied
        }

    // MAHIMA: Analyze population demographics to show expected counts
    if constexpr (ENABLE_DETAILED_CALCULATION_DEBUG) {
        if (context.has_risk_factor_inspector()) {
            context.get_risk_factor_inspector().analyze_population_demographics(context);
        }
    }

    // MAHIMA: Combine food and other risk factors into single adjustment call
    // This prevents the "invalid unordered_map<K, T> key" error by ensuring all factors
    // are processed in one adjustments map instead of separate maps
    auto combined_factors = combine_risk_factors();
    auto combined_ranges = combine_risk_factor_ranges();
    
    // MAHIMA: Use all combined factors (both food and other risk factors)
    // All factors are now initialized in person.risk_factors
    auto filtered_factors = combined_factors;
    auto filtered_ranges = combined_ranges;
    
    // Single call to adjust_risk_factors with filtered factors and their ranges
    adjust_risk_factors(context, filtered_factors, filtered_ranges, false);

    // Initialise everyone with policies and trends.
    for (auto &person : context.population()) {
        initialise_policies(person, context.random(), false);
        initialise_trends(context, person);
    }
    
    // MAHIMA: Adjust trended risk factor means using combined factors
    // Use the same combined factors and ranges for trended adjustment
    adjust_risk_factors(context, combined_factors, combined_ranges, true);

    // MAHIMA: Capture detailed calculation steps for debugging AFTER all adjustments
    // This ensures we have the final values including adjustments
    if constexpr (ENABLE_DETAILED_CALCULATION_DEBUG) {
        if (context.has_risk_factor_inspector()) {
            auto &inspector = context.get_risk_factor_inspector();
            // Capture all risk factors for all people after adjustments
            for (auto &person : context.population()) {
                for (size_t i = 0; i < names_.size(); i++) {
                    // Get the stored calculation details and write to CSV
                    inspector.capture_person_risk_factors(context, person, names_[i].to_string(), i);
                }
            }
        }
    }

    // Print risk factor summary once at the end
    std::string risk_factor_list;
    for (size_t i = 0; i < names_.size(); i++) {
        if (i > 0)
            risk_factor_list += ", ";
        risk_factor_list += names_[i].to_string();
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
            
            // MAHIMA: No debugging capture for newborns in update_risk_factors
            // Individual-level debugging only happens during initial population generation
        } else {
            // For existing people, only update sector and factors
            // update_sector(person, context.random());
            update_factors(context, person, context.random());
            
            // MAHIMA: No debugging capture for existing people updates
            // Individual-level debugging only happens during initial population generation
        }
    }

    // MAHIMA: Combine food and other risk factors into single adjustment call
    // This prevents the "invalid unordered_map<K, T> key" error by ensuring all factors
    // are processed in one adjustments map instead of separate maps
    auto combined_factors = combine_risk_factors();
    auto combined_ranges = combine_risk_factor_ranges();
    
    // MAHIMA: Use all combined factors (both food and other risk factors)
    // All factors are now initialized in person.risk_factors
    auto filtered_factors = combined_factors;
    auto filtered_ranges = combined_ranges;
    
    // Single call to adjust_risk_factors with filtered factors and their ranges
    adjust_risk_factors(context, filtered_factors, filtered_ranges, false);

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

    // MAHIMA: Adjust trended risk factor means using combined factors
    // Use the same combined factors and ranges for trended adjustment
    adjust_risk_factors(context, combined_factors, combined_ranges, true);
    

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

// MAHIMA: Set debug configuration for detailed calculation capture
void StaticLinearModel::set_debug_config(bool enabled, int age, core::Gender gender, 
                                        const std::string &risk_factor) {
    // This method can be called to configure debugging, but the actual configuration
    // is handled by the RiskFactorInspector. This is just a placeholder for now.
    // In a real implementation, you would need to pass this configuration to the inspector.
    std::cout << "\nMAHIMA: Debug configuration requested - Age: " << age 
              << ", Gender: " << (gender == core::Gender::male ? "male" : 
                                 gender == core::Gender::female ? "female" : "any")
              << ", Risk Factor: " << (risk_factor.empty() ? "any" : risk_factor)
              << ", Enabled: " << (enabled ? "true" : "false");
}

// MAHIMA: Initialize logistic factors for simulated mean calculation
void StaticLinearModel::initialize_logistic_factors() {
    // Clear any existing logistic factors
    logistic_factors_.clear();
    
    // Check each risk factor to see if it has a logistic model
    for (size_t i = 0; i < names_.size(); i++) {
        // A factor has a logistic model if its coefficients are not empty
        bool has_logistic_model = !(logistic_models_[i].coefficients.empty());
        
        if (has_logistic_model) {
            logistic_factors_.insert(names_[i]);
        } 
    }
    std::cout << "\nMAHIMA: Total factors with logistic models: " << logistic_factors_.size() << " out of " << names_.size();
}

void StaticLinearModel::initialise_factors(RuntimeContext &context, Person &person,
                                           Random &random) const {

    // Correlated residual sampling.
    auto residuals = compute_residuals(random, cholesky_);
    
    // MAHIMA: Store original residuals before Cholesky for debugging
    std::vector<double> original_residuals;
    if constexpr (ENABLE_DETAILED_CALCULATION_DEBUG) {
        if (context.has_risk_factor_inspector()) {
            // Generate original residuals before Cholesky transformation
            original_residuals.reserve(names_.size());
            for (size_t i = 0; i < names_.size(); i++) {
                original_residuals.push_back(random.next_normal(0.0, 1.0));
            }
        }
    }

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

// ... existing code ...

// STAGE 1: Determine if risk factor should be zero (only if logistic model exists)
if (has_logistic_model) {
    double zero_probability = calculate_zero_probability(person, i);
    
    // Sample from this probability to determine if risk factor should be zero
    // if logistic regression output = 1, risk factor value = 0
    double random_sample = random.next_double(); // Uniform random value between 0 and 1
    if (random_sample < zero_probability) {
        // Risk factor should be zero
        person.risk_factors[names_[i]] = 0.0;
        
        // MAHIMA: Store calculation details for Stage 1 (logistic regression)
        if constexpr (ENABLE_DETAILED_CALCULATION_DEBUG) {
            if (context.has_risk_factor_inspector()) {
                auto &inspector = context.get_risk_factor_inspector();
                // Store the calculation details for this person and risk factor
                inspector.store_calculation_details(person, names_[i].to_string(), i,
                                                 original_residuals.empty() ? 0.0 : original_residuals[i], 
                                                 residuals[i], expected, linear[i], residual,
                                                 stddev_[i], 0.0, lambda_[i], 0.0, // factor = 0, boxcox_result = 0
                                                 0.0, ranges_[i].lower(), ranges_[i].upper(), // factor_before_clamp = 0
                                                 0.0, // first_clamped_factor_value = 0
                                                 0.0, // simulated_mean (will be calculated during adjustment)
                                                 0.0, // factors_mean_delta (will be calculated during adjustment)
                                                 0.0, // value_after_adjustment_before_second_clamp (will be calculated during adjustment)
                                                 0.0); // final_value_after_second_clamp (will be calculated during adjustment)
            }
        }
        continue;
    }
}

// STAGE 2: Calculate non-zero risk factor value using BoxCox transformation
// (This code runs whether we have a logistic model or not)
double factor = linear[i] + residual * stddev_[i];
double boxcox_result = inverse_box_cox(factor, lambda_[i]);
double factor_before_clamp = expected * boxcox_result;
double first_clamped_factor_value = ranges_[i].clamp(factor_before_clamp);

// MAHIMA: Store calculation details for Stage 2 (BoxCox transformation)
if constexpr (ENABLE_DETAILED_CALCULATION_DEBUG) {
    if (context.has_risk_factor_inspector()) {
        auto &inspector = context.get_risk_factor_inspector();
        // Store the calculation details for this person and risk factor
        inspector.store_calculation_details(person, names_[i].to_string(), i,
                                         original_residuals.empty() ? 0.0 : original_residuals[i], 
                                         residuals[i], expected, linear[i], residual,
                                         stddev_[i], factor, lambda_[i], boxcox_result,
                                         factor_before_clamp, ranges_[i].lower(), ranges_[i].upper(), first_clamped_factor_value,
                                         0.0, // simulated_mean (will be calculated during adjustment)
                                         0.0, // factors_mean_delta (will be calculated during adjustment)
                                         0.0, // value_after_adjustment_before_second_clamp (will be calculated during adjustment)
                                         0.0); // final_value_after_second_clamp (will be calculated during adjustment)
    }
}

// Save risk factor
person.risk_factors[names_[i]] = first_clamped_factor_value;
    }
}

void StaticLinearModel::update_factors(RuntimeContext &context, Person &person,
                                       Random &random) const {

    // Correlated residual sampling.
    auto new_residuals = compute_residuals(random, cholesky_);

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
        double factor = linear[i] + residual * stddev_[i];
        double boxcox_result = inverse_box_cox(factor, lambda_[i]);
        double factor_before_clamp = expected * boxcox_result;
        double first_clamped_factor_value = ranges_[i].clamp(factor_before_clamp);

        // MAHIMA: No debugging capture during updates
        // Individual-level debugging only happens during initial population generation

        // Save risk factor
        person.risk_factors[names_[i]] = first_clamped_factor_value;
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
    auto residuals = compute_residuals(random, policy_cholesky_);

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
            factor += coefficient_value * log(value);
        }

        linear.emplace_back(factor);
    }

    return linear;
}

std::vector<double> StaticLinearModel::compute_residuals(Random &random,
                                                         const Eigen::MatrixXd &cholesky) const {
    std::vector<double> correlated_residuals{};
    correlated_residuals.reserve(names_.size());

    // Correlated samples using Cholesky decomposition.
    Eigen::VectorXd residuals{names_.size()};
    std::ranges::generate(residuals, [&random] { return random.next_normal(0.0, 1.0); });
    residuals = cholesky * residuals;

    // Save correlated residuals.
    for (size_t i = 0; i < names_.size(); i++) {
        correlated_residuals.emplace_back(residuals[i]);
    }

    return correlated_residuals;
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
    std::vector<LinearModelParams> logistic_models,
    std::vector<core::Identifier> other_names,
    std::vector<core::DoubleInterval> other_ranges)
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
      logistic_models_{std::move(logistic_models)},
      other_risk_factor_names_{std::move(other_names)}, other_risk_factor_ranges_{std::move(other_ranges)} {

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
        physical_activity_stddev_, get_physical_activity_models(), logistic_models_,
        other_risk_factor_names_, other_risk_factor_ranges_);
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
