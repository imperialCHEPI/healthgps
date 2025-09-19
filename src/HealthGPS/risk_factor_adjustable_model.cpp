#include "HealthGPS.Core/exception.h"

#include "risk_factor_adjustable_model.h"
#include "risk_factor_inspector.h"
#include "sync_message.h"

#include <iostream>
#include <iomanip>
#include <oneapi/tbb/parallel_for_each.h>
#include <utility>
#include <thread>
#include <chrono>
#include <unordered_set>

namespace { // anonymous namespace

/// @brief Defines the risk factors adjustment synchronisation message
using RiskFactorAdjustmentMessage = hgps::SyncDataMessage<hgps::RiskFactorSexAgeTable>;

/// @brief Defines the first statistical moment type
struct FirstMoment {
  public:
    void append(double value) noexcept {
        count_++;
        sum_ += value;
    }

    // Empty mean is NaN, so risk factors are not adjusted.
    double mean() const noexcept { return sum_ / count_; }

  private:
    int count_{};
    double sum_{};
};

} // anonymous namespace

namespace hgps {

RiskFactorAdjustableModel::RiskFactorAdjustableModel(
    std::shared_ptr<RiskFactorSexAgeTable> expected,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps)
    : expected_{std::move(expected)}, expected_trend_{std::move(expected_trend)},
      trend_steps_{std::move(trend_steps)} {}

double RiskFactorAdjustableModel::get_expected(RuntimeContext &context, core::Gender sex, int age,
                                               const core::Identifier &factor, OptionalRange range,
                                               bool apply_trend) const noexcept {
    // Get expected risk factor at age and sex.
    double expected = 0.0;
    if (expected_->contains(sex, factor)) {
        const auto &expected_by_age = expected_->at(sex, factor);
        if (static_cast<size_t>(age) < expected_by_age.size()) {
            expected = expected_by_age.at(age);
        }
    }

    // Apply expected trend.
    if (apply_trend && expected_trend_->contains(factor)) {
        int t = std::min(context.time_now() - context.start_time(), get_trend_steps(factor));
        expected *= pow(expected_trend_->at(factor), t);
    }

    // Clamp to an optionally specified range.
    if (range.has_value()) {
        expected = range.value().get().clamp(expected);
    }

    return expected;
}

void RiskFactorAdjustableModel::adjust_risk_factors(RuntimeContext &context,
                                                    const std::vector<core::Identifier> &factors,
                                                    OptionalRanges ranges, bool apply_trend) const {
    RiskFactorSexAgeTable adjustments;
    RiskFactorSexAgeTable simulated_means; // MAHIMA: Store simulated means for debugging

    // Flag to track if ranges are applied at least once
    // bool any_ranges_applied = false;

    // Baseline scenario: compute adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        // MAHIMA: Calculate simulated means using first_clamped_factor_value from stored details
        auto age_range = context.age_range();
        
        // Create a lambda function to get first_clamped_factor_value from stored calculation details
        auto get_first_clamped_value = [&context](const Person& person, const core::Identifier& factor) -> double {
            if (context.has_risk_factor_inspector()) {
                auto &inspector = context.get_risk_factor_inspector();
                RiskFactorInspector::CalculationDetails details;
                if (inspector.get_stored_calculation_details(person, factor.to_string(), details)) {
                    return details.first_clamped_factor_value;
                }
            }
            // Fallback to current risk factor value if no stored details
            if (person.risk_factors.contains(factor)) {
                return person.risk_factors.at(factor);
            }
            return 0.0;
        };
        
        simulated_means = calculate_simulated_mean_with_details(context.population(), age_range, factors, logistic_factors_, get_first_clamped_value);
        
        adjustments = calculate_adjustments(context, factors, ranges, apply_trend);    
    }

    // Intervention scenario: receive adjustments from baseline scenario.
    else {
            auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
            if (!message.has_value()) {
            throw core::HgpsException(
                "Simulation out of sync, receive baseline adjustments message has timed out");
            }

        auto &basePtr = message.value();
        auto *messagePrt = dynamic_cast<RiskFactorAdjustmentMessage *>(basePtr.get());
            if (!messagePrt) {
            throw core::HgpsException(
                "Simulation out of sync, failed to receive a baseline adjustments message");
            }

            adjustments = messagePrt->data();
    }


    // All scenarios: apply adjustments to population.
    
    auto &pop = context.population();
    
    // MAHIMA: Store original values for verification (only for baseline scenario)
    // Only for income and physical activity to verify that they are adjusted to factors mean
    std::unordered_map<size_t, double> original_income;
    std::unordered_map<size_t, double> original_physical_activity;
    if (context.scenario().type() == ScenarioType::baseline && context.time_now() == context.start_time()) {
        for (const auto& person : pop) {
            if (!person.is_active()) continue;
            if (std::find(factors.begin(), factors.end(), "income"_id) != factors.end()) {
                original_income[person.id()] = person.income_continuous;
            }
            if (std::find(factors.begin(), factors.end(), "physicalactivity"_id) != factors.end()) {
                original_physical_activity[person.id()] = person.physical_activity;
            }
        }
    }
    
        tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }

        for (size_t factor_index = 0; factor_index < factors.size(); factor_index++) {
            const auto &factor = factors[factor_index];
            
            // MAHIMA: Skip income and physicalactivity - they are handled separately below
            if (factor.to_string() == "income" || factor.to_string() == "physicalactivity") {
                continue;
            }
            
    // Check if factor exists in adjustments map
    if (!adjustments.contains(person.gender)) {
        std::cout << "\nERROR: Gender " << (person.gender == core::Gender::male ? "Male" : "Female") << " not found in adjustments map";
        continue;
    }
    
    if (!adjustments.at(person.gender).contains(factor)) {
        std::cout << "\nERROR: Factor " << factor.to_string() << " not found in adjustments map for gender " << (person.gender == core::Gender::male ? "Male" : "Female");
        continue;
    }

            // MAHIMA: Get first_clamped_factor_value from stored calculation details
            double first_clamped_factor_value = 0.0;
            if (person.risk_factors.contains(factor)) {
                first_clamped_factor_value = person.risk_factors.at(factor);
            }

            // MAHIMA: Preserve zero values from two-stage modeling - don't adjust them
            if (first_clamped_factor_value == 0.0) {
                // Skip adjustment for zero values to preserve two-stage modeling results
                continue;
            }

            // Check if age is within bounds
            const auto &age_vector = adjustments.at(person.gender, factor);
            if (person.age >= static_cast<int>(age_vector.size())) {
                std::cout << "\nWARNING: Person age " << person.age << " is out of bounds for factor " 
                          << factor.to_string() << " (max age: " 
                          << age_vector.size() - 1 << ")";
            }
            
            // MAHIMA: Get the simulated mean for this age/gender/factor
            double simulated_mean = 0.0;
            if (simulated_means.contains(person.gender) && simulated_means.at(person.gender).contains(factor)) {
                const auto &sim_mean_vector = simulated_means.at(person.gender, factor);
                if (person.age < static_cast<int>(sim_mean_vector.size())) {
                    simulated_mean = sim_mean_vector.at(person.age);
                }
            }
            
            // MAHIMA: Get the expected value from factors mean table
            double expected_value = 0.0;
            if (expected_->contains(person.gender, factor)) {
                const auto &expected_vector = expected_->at(person.gender, factor);
                if (person.age < static_cast<int>(expected_vector.size())) {
                    expected_value = expected_vector.at(person.age);
                }
            }
            
            // MAHIMA: Calculate the 4 new values
            double factors_mean_delta = expected_value - simulated_mean;
            double value_after_adjustment_before_second_clamp = first_clamped_factor_value + factors_mean_delta;
            
            // Apply range clamping to get final value
            double final_value_after_second_clamp = value_after_adjustment_before_second_clamp;
            if (ranges.has_value()) {
                final_value_after_second_clamp = ranges.value().get().at(factor_index).clamp(value_after_adjustment_before_second_clamp);
            }

            // Set the adjusted value with any additional validation
            person.set_risk_factor(context, factor, final_value_after_second_clamp);
        }

        // MAHIMA: Special handling for Income and PhysicalActivity
        // These are demographic attributes, not regular risk factors
        // Map income_continuous to income (lowercase)
        if (std::find(factors.begin(), factors.end(), "income"_id) != factors.end()) {
            if (adjustments.contains(person.gender) && adjustments.at(person.gender).contains("income"_id)) {
                // Check if age is within bounds
                const auto &income_vector = adjustments.at(person.gender, "income"_id);
                if (person.age < static_cast<int>(income_vector.size())) {
                    double delta = income_vector.at(person.age);
                    double original_income = person.income_continuous;
                    person.income_continuous += delta;
                    
                    // MAHIMA: Update stored calculation details for income
                    if (context.has_risk_factor_inspector()) {
                        auto &inspector = context.get_risk_factor_inspector();
                        
                        // Get the actual simulated mean for income from the calculated table
                        double income_sim_mean = 0.0;
                        if (simulated_means.contains(person.gender) && simulated_means.at(person.gender).contains("income"_id)) {
                            const auto &sim_mean_vector = simulated_means.at(person.gender, "income"_id);
                            if (person.age < static_cast<int>(sim_mean_vector.size())) {
                                income_sim_mean = sim_mean_vector.at(person.age);
                            }
                        }
                        
                        // Get expected income value
                        double income_expected_value = 0.0;
                        if (expected_->contains(person.gender, "income"_id)) {
                            const auto &expected_vector = expected_->at(person.gender, "income"_id);
                            if (person.age < static_cast<int>(expected_vector.size())) {
                                income_expected_value = expected_vector.at(person.age);
                            }
                        }
                        
                        inspector.update_calculation_details_with_adjustments(person, "income",
                                                                             income_expected_value, income_sim_mean, delta, person.income_continuous, person.income_continuous);
                    }
                }
            }
        }

        // Map physical_activity to physicalactivity (lowercase)
        if (std::find(factors.begin(), factors.end(), "physicalactivity"_id) != factors.end()) {
            if (adjustments.contains(person.gender) && adjustments.at(person.gender).contains("physicalactivity"_id)) {
                // Check if age is within bounds
                const auto &pa_vector = adjustments.at(person.gender, "physicalactivity"_id);
                if (person.age < static_cast<int>(pa_vector.size())) {
                    double delta = pa_vector.at(person.age);
                    double original_pa = person.physical_activity;
                    person.physical_activity += delta;
                    
                    // MAHIMA: Update stored calculation details for physical activity
                    if (context.has_risk_factor_inspector()) {
                        auto &inspector = context.get_risk_factor_inspector();
                        
                        // Get the actual simulated mean for physical activity from the calculated table
                        double pa_sim_mean = 0.0;
                        if (simulated_means.contains(person.gender) && simulated_means.at(person.gender).contains("physicalactivity"_id)) {
                            const auto &sim_mean_vector = simulated_means.at(person.gender, "physicalactivity"_id);
                            if (person.age < static_cast<int>(sim_mean_vector.size())) {
                                pa_sim_mean = sim_mean_vector.at(person.age);
                            }
                        }
                        
                        // Get expected physical activity value
                        double pa_expected_value = 0.0;
                        if (expected_->contains(person.gender, "physicalactivity"_id)) {
                            const auto &expected_vector = expected_->at(person.gender, "physicalactivity"_id);
                            if (person.age < static_cast<int>(expected_vector.size())) {
                                pa_expected_value = expected_vector.at(person.age);
                            }
                        }
                        
                        inspector.update_calculation_details_with_adjustments(person, "physicalactivity",
                                                                             pa_expected_value, pa_sim_mean, delta, person.physical_activity, person.physical_activity);
                    }
                }
            }
        }
        
        // MAHIMA: Update stored calculation details for ALL risk factors for this person
        // This ensures we capture debugging info for all people AFTER adjustments are applied
        if (context.has_risk_factor_inspector()) {
            auto &inspector = context.get_risk_factor_inspector();
            
            // Process all factors for debugging, regardless of whether they were adjusted
            for (size_t factor_index = 0; factor_index < factors.size(); factor_index++) {
                const auto &factor = factors[factor_index];
                
                // Skip income and physicalactivity - they are handled separately above
                if (factor.to_string() == "income" || factor.to_string() == "physicalactivity") {
                    continue;
                }
                
                // Get first_clamped_factor_value from stored calculation details
                double first_clamped_factor_value = 0.0;
                RiskFactorInspector::CalculationDetails stored_details;
                if (context.has_risk_factor_inspector()) {
                    auto &inspector = context.get_risk_factor_inspector();
                    if (inspector.get_stored_calculation_details(person, factor.to_string(), stored_details)) {
                        first_clamped_factor_value = stored_details.first_clamped_factor_value;
                    }
                }
                // Fallback to current risk factor value if no stored details
                if (first_clamped_factor_value == 0.0 && person.risk_factors.contains(factor)) {
                    first_clamped_factor_value = person.risk_factors.at(factor);
                }
                
                // Get the expected value from factors mean table (same for all people of this age/gender)
                double expected_value = 0.0;
                if (expected_->contains(person.gender, factor)) {
                    const auto &expected_vector = expected_->at(person.gender, factor);
                    if (person.age < static_cast<int>(expected_vector.size())) {
                        expected_value = expected_vector.at(person.age);
                    }
                }
                
                // Get the actual simulated mean from the calculated table (same for all people of this age/gender)
                double simulated_mean = 0.0;
                if (simulated_means.contains(person.gender) && simulated_means.at(person.gender).contains(factor)) {
                    const auto &sim_mean_vector = simulated_means.at(person.gender, factor);
                    if (person.age < static_cast<int>(sim_mean_vector.size())) {
                        simulated_mean = sim_mean_vector.at(person.age);
                    }
                }
                
                // MAHIMA: Preserve zero values from two-stage modeling - don't adjust them
                if (first_clamped_factor_value == 0.0) {
                    // For zero values, set adjustment values to 0 but keep expected_value and simulated_mean
                    double factors_mean_delta = expected_value - simulated_mean;
                    inspector.update_calculation_details_with_adjustments(person, factor.to_string(),
                                                                         expected_value, simulated_mean, factors_mean_delta, 0.0, 0.0);
                    continue;
                }
                
                // Calculate the 4 new values
                double factors_mean_delta = expected_value - simulated_mean;
                double value_after_adjustment_before_second_clamp = first_clamped_factor_value + factors_mean_delta;
                
                // Apply range clamping to get final value
                double final_value_after_second_clamp = value_after_adjustment_before_second_clamp;
                if (ranges.has_value()) {
                    final_value_after_second_clamp = ranges.value().get().at(factor_index).clamp(value_after_adjustment_before_second_clamp);
                }
                
                // Update the stored calculation details with the final adjusted values
                inspector.update_calculation_details_with_adjustments(person, factor.to_string(),
                                                                     expected_value, simulated_mean, factors_mean_delta, 
                                                                     value_after_adjustment_before_second_clamp, final_value_after_second_clamp);
            }
        }
    });

    // Baseline scenario: send adjustments to intervention scenario.
    if (context.scenario().type() == ScenarioType::baseline) {
            context.scenario().channel().send(std::make_unique<RiskFactorAdjustmentMessage>(
                context.current_run(), context.time_now(), std::move(adjustments)));
    }
}

int RiskFactorAdjustableModel::get_trend_steps(const core::Identifier &factor) const {
    return trend_steps_->at(factor);
}

void RiskFactorAdjustableModel::set_logistic_factors(const std::unordered_set<core::Identifier> &logistic_factors) {
    logistic_factors_ = logistic_factors;
}

RiskFactorSexAgeTable
RiskFactorAdjustableModel::calculate_adjustments(RuntimeContext &context,
                                                 const std::vector<core::Identifier> &factors,
                                                 OptionalRanges ranges, bool apply_trend) const {
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_adjustments - Starting";
    auto age_range = context.age_range();
    auto age_count = age_range.upper() + 1;

    // MAHIMA: Calculate simulated means using first_clamped_factor_value from stored details
    // Create a lambda function to get first_clamped_factor_value from stored calculation details
    auto get_first_clamped_value = [&context](const Person& person, const core::Identifier& factor) -> double {
        if (context.has_risk_factor_inspector()) {
            auto &inspector = context.get_risk_factor_inspector();
            RiskFactorInspector::CalculationDetails details;
            if (inspector.get_stored_calculation_details(person, factor.to_string(), details)) {
                return details.first_clamped_factor_value;
            }
        }
    };
    
    auto simulated_means = calculate_simulated_mean_with_details(context.population(), age_range, factors, logistic_factors_, get_first_clamped_value);

    // Compute adjustments.
    auto adjustments = RiskFactorSexAgeTable{};
    for (const auto &[sex, simulated_means_by_sex] : simulated_means) {
        for (size_t i = 0; i < factors.size(); i++) {
            const core::Identifier &factor = factors[i];

            // Check if this factor exists in the expected table
            if (!expected_->contains(sex, factor)) {
                std::cout << "\nWARNING - Factor " << factor.to_string() << " not found in expected table for gender " << (sex == core::Gender::male ? "Male" : "Female") << " - skipping";
                continue;
            }

            OptionalRange range;
            if (ranges.has_value()) {
                range = OptionalRange{ranges.value().get().at(i)};
            }

            adjustments.emplace(sex, factor, std::vector<double>(age_count));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                double expect = get_expected(context, sex, age, factor, range, apply_trend);
                double sim_mean = simulated_means_by_sex.at(factor).at(age);

                // Delta should remain zero if simulated mean is NaN.
                double delta = 0.0;

                // Else, delta is the distance from expected value.
                if (!std::isnan(sim_mean)) {
                    delta = expect - sim_mean;
                }

                adjustments.at(sex, factor).at(age) = delta;
            }
        }
    }
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_adjustments - Adjustments
    // computed"; std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_adjustments -
    // Completed";

    return adjustments;
}

// MAHIMA: New overloaded function that can access stored calculation details
RiskFactorSexAgeTable RiskFactorAdjustableModel::calculate_simulated_mean_with_details(
    Population &population, const core::IntegerInterval age_range,
    const std::vector<core::Identifier> &factors,
    const std::unordered_set<core::Identifier> &logistic_factors,
    const std::function<double(const Person&, const core::Identifier&)>& get_value_func) {
    auto age_count = age_range.upper() + 1;

    // MAHIMA: Track excluded values for debugging
    std::unordered_map<core::Identifier, int> excluded_counts;
    std::unordered_map<core::Identifier, int> total_counts;
    for (const auto &factor : factors) {
        excluded_counts[factor] = 0;
        total_counts[factor] = 0;
    }

    // Compute first moments.
    auto moments = UnorderedMap2d<core::Gender, core::Identifier, std::vector<FirstMoment>>{};

    for (const auto &person : population) {
        if (!person.is_active()) {
            continue;
        }

        for (const auto &factor : factors) {
            if (!moments.contains(person.gender, factor)) {
                moments.emplace(person.gender, factor, std::vector<FirstMoment>(age_count));
            }

            double value = get_value_func(person, factor);
            bool has_value = true; // The function should handle missing values

            // MAHIMA: Apply logistic model logic for simulated mean calculation
            // For factors WITH logistic models: only include non-zero values
            // For factors WITHOUT logistic models: include all values (including zeros)
            if (has_value) {
                total_counts[factor]++;
                bool should_include = true;
                if (logistic_factors.contains(factor)) {
                    // Factor has logistic model - only include non-zero values
                    if (value == 0)
                        should_include = false;
                    if (!should_include) {
                        excluded_counts[factor]++;
                    }
                }
                // If factor doesn't have logistic model, include all values (including zeros)

                if (should_include) {
                moments.at(person.gender, factor).at(person.age).append(value);
                }
            }
        }
    }

    // Compute means.
    auto means = RiskFactorSexAgeTable{};
    for (const auto &[sex, moments_by_sex] : moments) {
        for (const auto &factor : factors) {
            means.emplace(sex, factor, std::vector<double>(age_count));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                double value = moments_by_sex.at(factor).at(age).mean();
                means.at(sex, factor).at(age) = value;
            }
        }
    }

    return means;
}

RiskFactorSexAgeTable RiskFactorAdjustableModel::calculate_simulated_mean(
    Population &population, const core::IntegerInterval age_range,
    const std::vector<core::Identifier> &factors,
    const std::unordered_set<core::Identifier> &logistic_factors) {
    auto age_count = age_range.upper() + 1;

    // MAHIMA: Track excluded values for debugging
    std::unordered_map<core::Identifier, int> excluded_counts;
    std::unordered_map<core::Identifier, int> total_counts;
    for (const auto &factor : factors) {
        excluded_counts[factor] = 0;
        total_counts[factor] = 0;
    }

    // Compute first moments.
    auto moments = UnorderedMap2d<core::Gender, core::Identifier, std::vector<FirstMoment>>{};

    for (const auto &person : population) {
        if (!person.is_active()) {
            continue;
        }

        for (const auto &factor : factors) {
            if (!moments.contains(person.gender, factor)) {
                moments.emplace(person.gender, factor, std::vector<FirstMoment>(age_count));
            }

            double value = 0.0;
            bool has_value = false;

            // Special handling for income and physical activity
            if (factor.to_string() == "income") {
                value = person.income_continuous;
                has_value = true;
            } else if (factor.to_string() == "physicalactivity") {
                value = person.physical_activity;
                has_value = true;
            } else if (person.risk_factors.contains(factor)) {
                // MAHIMA: Use first_clamped_factor_value from stored calculation details if available
                // Otherwise fall back to the current risk factor value
                value = person.risk_factors.at(factor);
                has_value = true;
            } else {
                // MAHIMA: Person doesn't have this risk factor (e.g., set to 0 by logistic regression)
                // Still include them in the mean calculation with value 0.0
                value = 0.0;
                has_value = true;
            }
            // MAHIMA: Apply logistic model logic for simulated mean calculation
            // For factors WITH logistic models: only include non-zero values
            // For factors WITHOUT logistic models: include all values (including zeros)
            if (has_value) {
                total_counts[factor]++;
                bool should_include = true;
                if (logistic_factors.contains(factor)) {
                    // Factor has logistic model - only include non-zero values
                    if (value == 0)
                        should_include = false;
                    if (!should_include) {
                        excluded_counts[factor]++;
                    }
                }
                // If factor doesn't have logistic model, include all values (including zeros)

                if (should_include) {
                moments.at(person.gender, factor).at(person.age).append(value);
                }
            }
        }
    }

    // Compute means.
    auto means = RiskFactorSexAgeTable{};
    for (const auto &[sex, moments_by_sex] : moments) {
        for (const auto &factor : factors) {
            means.emplace(sex, factor, std::vector<double>(age_count));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                double value = moments_by_sex.at(factor).at(age).mean();
                means.at(sex, factor).at(age) = value;
            }
        }
    }

    // MAHIMA: Print excluded values summary only once
    static bool debug_summary_printed = false;
    if (!debug_summary_printed) {
        std::cout << "\n=== SIMULATED MEAN CALCULATION - EXCLUDED VALUES SUMMARY ===";
        for (const auto &factor : factors) {
            if (logistic_factors.contains(factor)) {
                std::cout << "\n"
                          << factor.to_string()
                          << " (HAS logistic model): " << excluded_counts[factor]
                          << " zero values excluded out of " << total_counts[factor]
                          << " total values (" << std::fixed << std::setprecision(1)
                          << (total_counts[factor] > 0
                                  ? (100.0 * excluded_counts[factor] / total_counts[factor])
                                  : 0.0)
                          << "% excluded)";
            } else {
                std::cout << "\n"
                          << factor.to_string()
                          << " (NO logistic model): " << excluded_counts[factor]
                          << " zero values excluded out of " << total_counts[factor]
                          << " total values (" << std::fixed << std::setprecision(1)
                          << (total_counts[factor] > 0
                                  ? (100.0 * excluded_counts[factor] / total_counts[factor])
                                  : 0.0)
                          << "% excluded)";
            }
        }
        std::cout << "\n===============================================================";
        
        // MAHIMA: Debug output to show some calculated means
        std::cout << "\n=== DEBUG: Sample calculated means ===";
        for (const auto &[sex, moments_by_sex] : moments) {
            for (const auto &factor : factors) {
                if (factor.to_string() == "foodalcohol" && sex == core::Gender::female) {
                    std::cout << "\n" << factor.to_string() << " (female):";
                    for (int age = 15; age <= 20; age++) {
                        if (age < static_cast<int>(moments_by_sex.at(factor).size())) {
                            double mean_val = moments_by_sex.at(factor).at(age).mean();
                            std::cout << "\n  Age " << age << ": " << mean_val;
                        }
                    }
                }
            }
        }
        std::cout << "\n=====================================";
        
        debug_summary_printed = true;
    }

    return means;
}


RiskFactorAdjustableModelDefinition::RiskFactorAdjustableModelDefinition(
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps)
    : expected_{std::move(expected)}, expected_trend_{std::move(expected_trend)},
      trend_steps_{std::move(trend_steps)} {

    if (expected_->empty()) {
        throw core::HgpsException("Risk factor expected value mapping is empty");
    }
    if (expected_trend_->empty()) {
        throw core::HgpsException("Risk factor expected trend mapping is empty");
    }
    if (trend_steps_->empty()) {
        throw core::HgpsException("Risk factor trend steps mapping is empty");
    }
}

} // namespace hgps

