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
        // MAHIMA: Calculate simulated means for debugging
        auto age_range = context.age_range();
        simulated_means = calculate_simulated_mean(context.population(), age_range, factors);
        
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

            // MAHIMA: Check if the person has this risk factor before accessing it
            if (!person.risk_factors.contains(factor)) {
        std::cout << "\nWARNING: Person ID " << person.id() << " does not have risk factor " 
                         << factor.to_string();
            }

            // MAHIMA: If adjustment for risk factors that are currently 0 (in case something is not initialized)
            // This preserves zero values set by logistic regression (two-stage modeling)
            double original_value = person.risk_factors.at(factor);
            // MAHIMA: Preserve zero values from two-stage modeling - don't adjust them
            if (original_value == 0.0) {
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
            
            double delta = age_vector.at(person.age);
            double value = original_value + delta;

            // Apply range clamping if ranges are provided
            if (ranges.has_value()) {
                // Make sure we're using the right index for the range
                value = ranges.value().get().at(factor_index).clamp(value);
                // any_ranges_applied = true; // Mark that ranges were applied at least once
            }

            // Set the adjusted value with any additional validation
            person.set_risk_factor(context, factor, value);
        }
        
        // MAHIMA: Update stored calculation details for ALL risk factors for this person
        // This ensures we capture debugging info for all people, not just those with adjustments
        if (context.has_risk_factor_inspector()) {
            auto &inspector = context.get_risk_factor_inspector();
            
            // Process all factors for debugging, regardless of whether they were adjusted
            for (size_t factor_index = 0; factor_index < factors.size(); factor_index++) {
                const auto &factor = factors[factor_index];
                
                // Skip income and physicalactivity - they are handled separately below
                if (factor.to_string() == "income" || factor.to_string() == "physicalactivity") {
                    continue;
                }
                
                // Get the adjustment delta for this age/gender/factor
                double adjustment_delta = 0.0;
                if (adjustments.contains(person.gender) && adjustments.at(person.gender).contains(factor)) {
                    const auto &adjustment_vector = adjustments.at(person.gender, factor);
                    if (person.age < static_cast<int>(adjustment_vector.size())) {
                        adjustment_delta = adjustment_vector.at(person.age);
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
                
                // Get the current value of the risk factor
                double current_value = 0.0;
                if (person.risk_factors.contains(factor)) {
                    current_value = person.risk_factors.at(factor);
                }
                
                // Update the stored calculation details
                inspector.update_calculation_details_with_adjustments(person, factor.to_string(),
                                                                     simulated_mean, adjustment_delta, current_value, current_value);
            }
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
                        
                        inspector.update_calculation_details_with_adjustments(person, "income",
                                                                             income_sim_mean, delta, person.income_continuous, person.income_continuous);
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
                        
                        inspector.update_calculation_details_with_adjustments(person, "physicalactivity",
                                                                             pa_sim_mean, delta, person.physical_activity, person.physical_activity);
                    }
                }
            }
        }
    });
    
    // MAHIMA: Verification box - show before/after values for sample of 6 people from different ages and genders
    if (context.scenario().type() == ScenarioType::baseline && context.time_now() == context.start_time()) {
        std::cout << "\n\n=== STATIC LINEAR MODEL VERIFICATION BOX (Sample of 6 people from different ages and genders) ===";
        std::cout << "\nPerson ID | Age | Gender | INCOME BEFORE | INCOME AFTER | INCOME DELTA | PHYSICAL ACTIVITY BEFORE | PHYSICAL ACTIVITY AFTER | PHYSICAL ACTIVITY DELTA";
        std::cout << "\n----------|-----|--------|---------------|--------------|-------------|-------------------------|------------------------|----------------------";
        
        // Sample from different ages and genders: (0,M), (0,F), (25,M), (25,F), (50,M), (50,F)
        std::vector<std::pair<int, core::Gender>> target_combinations = {
            {0, core::Gender::male}, {0, core::Gender::female},
            {25, core::Gender::male}, {25, core::Gender::female},
            {50, core::Gender::male}, {50, core::Gender::female}
        };
        int sample_count = 0;
        
        for (auto [target_age, target_gender] : target_combinations) {
            if (sample_count >= 6) break;
            
            for (const auto& person : pop) {
                if (!person.is_active() || person.age != target_age || person.gender != target_gender) continue;
                
                std::cout << "\n" << std::setw(9) << static_cast<int>(person.id()) 
                          << " | " << std::setw(3) << person.age 
                          << " | " << std::setw(6) << (person.gender == core::Gender::male ? "Male" : "Female");
                
                // Show income before/after values
                if (std::find(factors.begin(), factors.end(), "income"_id) != factors.end() && 
                    original_income.find(person.id()) != original_income.end()) {
                    double before = original_income[person.id()];
                    double after = person.income_continuous;
                    double delta = after - before;
                    std::cout << " | " << std::setw(13) << std::fixed << std::setprecision(2) << before
                              << " | " << std::setw(12) << std::fixed << std::setprecision(2) << after
                              << " | " << std::setw(11) << std::fixed << std::setprecision(2) << delta;
                } else {
                    std::cout << " | " << std::setw(13) << "N/A" << " | " << std::setw(12) << "N/A" << " | " << std::setw(11) << "N/A";
                }
                
                // Show physical activity before/after values
                if (std::find(factors.begin(), factors.end(), "physicalactivity"_id) != factors.end() && 
                    original_physical_activity.find(person.id()) != original_physical_activity.end()) {
                    double before = original_physical_activity[person.id()];
                    double after = person.physical_activity;
                    double delta = after - before;
                    std::cout << " | " << std::setw(23) << std::fixed << std::setprecision(2) << before
                              << " | " << std::setw(22) << std::fixed << std::setprecision(2) << after
                              << " | " << std::setw(20) << std::fixed << std::setprecision(2) << delta;
                } else {
                    std::cout << " | " << std::setw(23) << "N/A" << " | " << std::setw(22) << "N/A" << " | " << std::setw(20) << "N/A";
                }
                
                sample_count++;
                break; // Found one person of this age/gender, move to next combination
            }
        }
        
        // Debug: Show expected vs simulated values for income to understand adjustments
        if (expected_->contains(core::Gender::male) && expected_->at(core::Gender::male).contains("income"_id)) {
            std::cout << "\n\n=== DEBUG: Income Expected vs Simulated Analysis ===";
            auto& male_expected_income = expected_->at(core::Gender::male, "income"_id);
            std::cout << "\nMale expected income values (ages 0,25,50): ";
            for (int age : {0, 25, 50}) {
                if (age < static_cast<int>(male_expected_income.size())) {
                    std::cout << "age" << age << "=" << std::fixed << std::setprecision(2) << male_expected_income[age] << " ";
                }
            }
            
            // Calculate simulated means for income
            std::cout << "\nMale simulated income means (ages 0,25,50): ";
            for (int age : {0, 25, 50}) {
                double sum = 0.0;
                int count = 0;
                for (const auto& person : pop) {
                    if (person.is_active() && person.gender == core::Gender::male && person.age == age) {
                        sum += person.income_continuous;
                        count++;
                    }
                }
                double mean = (count > 0) ? sum / count : 0.0;
                std::cout << "age" << age << "=" << std::fixed << std::setprecision(2) << mean << " ";
            }
        }
        
        std::cout << "\n======================================================================================================================================================\n";
    }

    // Baseline scenario: send adjustments to intervention scenario.
    if (context.scenario().type() == ScenarioType::baseline) {
            context.scenario().channel().send(std::make_unique<RiskFactorAdjustmentMessage>(
                context.current_run(), context.time_now(), std::move(adjustments)));
    }
}

int RiskFactorAdjustableModel::get_trend_steps(const core::Identifier &factor) const {
    return trend_steps_->at(factor);
}

RiskFactorSexAgeTable
RiskFactorAdjustableModel::calculate_adjustments(RuntimeContext &context,
                                                 const std::vector<core::Identifier> &factors,
                                                 OptionalRanges ranges, bool apply_trend) const {
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_adjustments - Starting";
    auto age_range = context.age_range();
    auto age_count = age_range.upper() + 1;

    // Compute simulated means.
    auto simulated_means = calculate_simulated_mean(context.population(), age_range, factors);

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

RiskFactorSexAgeTable
RiskFactorAdjustableModel::calculate_simulated_mean(Population &population,
                                                    const core::IntegerInterval age_range,
                                                    const std::vector<core::Identifier> &factors) {
    auto age_count = age_range.upper() + 1;

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
                value = person.risk_factors.at(factor);
                has_value = true;
            }

            if (has_value) {
                moments.at(person.gender, factor).at(person.age).append(value);
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
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_simulated_mean - Means computed";
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_simulated_mean - Completed";

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

