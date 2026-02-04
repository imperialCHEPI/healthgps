#include "HealthGPS.Core/exception.h"

#include "risk_factor_adjustable_model.h"

#include "sync_message.h"
#include <fmt/core.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <oneapi/tbb/parallel_for_each.h>
#include <optional>
#include <utility>

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

/// @brief Get factor value from person for simulated mean (income, PA, or risk_factors).
std::optional<double> get_factor_value_for_person(const hgps::Person &person,
                                                  const hgps::core::Identifier &factor) {
    const std::string key = factor.to_string();
    if (key == "income" || key == "Income") {
        if (person.risk_factors.contains(factor)) {
            return person.risk_factors.at(factor);
        }
        return std::nullopt;
    }
    if (key == "PhysicalActivity") {
        return person.physical_activity;
    }
    if (person.risk_factors.contains(factor)) {
        return person.risk_factors.at(factor);
    }
    return std::nullopt;
}

/// @brief Whether to include this value in simulated mean (exclude zeros for logistic factors).
bool should_include_in_simulated_mean(double value, const hgps::core::Identifier &factor,
                                     const std::unordered_set<hgps::core::Identifier> &logistic_factors) {
    if (!logistic_factors.contains(factor)) {
        return true;
    }
    return value != 0;
}

/// @brief Print excluded-values summary once (thread-local).
void print_excluded_summary_once(
    const std::vector<hgps::core::Identifier> &factors,
    const std::unordered_set<hgps::core::Identifier> &logistic_factors,
    const std::unordered_map<hgps::core::Identifier, int> &excluded_counts,
    const std::unordered_map<hgps::core::Identifier, int> &total_counts) {
    thread_local static bool summary_printed = false;
    if (summary_printed) {
        return;
    }
    const hgps::core::Identifier income_id("income");
    const hgps::core::Identifier pa_id("PhysicalActivity");
    std::cout << "\n=== SIMULATED MEAN CALCULATION - EXCLUDED VALUES SUMMARY ===";
    std::cout << "\nLogistic factors set size: " << logistic_factors.size();
    for (const auto &factor : factors) {
        if (factor == income_id || factor == pa_id) {
            continue;
        }
        const int excluded = excluded_counts.at(factor);
        const int total = total_counts.at(factor);
        const double pct = (total > 0) ? (100.0 * excluded / total) : 0.0;
        const char *tag = logistic_factors.contains(factor) ? "HAS logistic model" : "NO logistic model";
        std::cout << "\n" << factor.to_string() << " (" << tag << "): " << excluded
                  << " zero values excluded out of " << total << " total values ("
                  << std::fixed << std::setprecision(1) << pct << "% excluded)";
    }
    std::cout << "\n===============================================================";
    std::cout.flush();
    summary_printed = true;
}

} // anonymous namespace

namespace hgps {

RiskFactorAdjustableModel::RiskFactorAdjustableModel(
    std::shared_ptr<RiskFactorSexAgeTable> expected,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps, TrendType trend_type,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, double>>
        expected_income_trend_decay_factors)
    : expected_{std::move(expected)}, expected_trend_{std::move(expected_trend)},
      trend_steps_{std::move(trend_steps)}, trend_type_{trend_type},
      expected_income_trend_{std::move(expected_income_trend)},
      expected_income_trend_decay_factors_{std::move(expected_income_trend_decay_factors)} {}

double RiskFactorAdjustableModel::get_expected(RuntimeContext &context, core::Gender sex, int age,
                                               const core::Identifier &factor, OptionalRange range,
                                               bool apply_trend) const {
    if (!expected_->contains(sex, factor)) {
        throw core::HgpsException(fmt::format(
            "Expected value not found for factor '{}' (sex={}) in factors mean table. "
            "Ensure FactorsMean.Male.csv and FactorsMean.Female.csv have a column for this factor.",
            factor.to_string(), sex == core::Gender::male ? "male" : "female"));
    }
    double expected = expected_->at(sex, factor).at(age);

    // Apply trend to expected value based on trend type
    if (apply_trend && trend_type_ != TrendType::Null) {
        int elapsed_time = context.time_now() - context.start_time();

        switch (trend_type_) {
        case TrendType::Null:
            // No trends applied to factors mean adjustment
            break;

        case TrendType::UPFTrend: {
            // Apply regular UPF trend to factors mean adjustment
            // Formula: factors_mean_T = factors_mean × ExpectedTrend^(T-T0)
            if (expected_trend_) {
                int t = std::min(elapsed_time, get_trend_steps(factor));
                expected *= pow(expected_trend_->at(factor), t);
            }
            break;
        }

        case TrendType::IncomeTrend: {
            // Apply income trend to factors mean adjustment
            // Formula: factors_mean_T = factors_mean × ExpectedIncomeTrend × e^(b×(T-T0)) for T >
            // T0
            if (elapsed_time > 0) { // Only apply from second year (T > T0)
                // Check if income trend data is available
                if (expected_income_trend_ && expected_income_trend_decay_factors_) {
                    double decay_factor = expected_income_trend_decay_factors_->at(factor);
                    double expected_income_trend = expected_income_trend_->at(factor);
                    expected *= expected_income_trend * exp(decay_factor * elapsed_time);
                }
                // If income trend data is not available, skip income trend application
            }
            // For T = T0 (first year), no income trend is applied (expected remains unchanged)
            break;
        }
        }
    }

    // Clamp expected value to an optionally specified range.
    if (range.has_value()) {
        expected = range.value().get().clamp(expected);
    }

    return expected;
}

void RiskFactorAdjustableModel::adjust_risk_factors(RuntimeContext &context,
                                                    const std::vector<core::Identifier> &factors,
                                                    OptionalRanges ranges, bool apply_trend) const {
    RiskFactorSexAgeTable adjustments;

    // Baseline scenatio: compute adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        adjustments = calculate_adjustments(context, factors, ranges, apply_trend);
    }

    // Intervention scenario: receive adjustments from baseline scenario.
    else {
        auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
        if (!message.has_value()) {
            std::cout << "\n[SYNC] Intervention: Message not received, timeout occurred!";
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
    tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }

        for (size_t i = 0; i < factors.size(); i++) {
            const core::Identifier &factor = factors[i];
            double delta = adjustments.at(person.gender, factor).at(person.age);
            std::string factor_name = factor.to_string();
            std::string factor_name_lower = factor_name;
            std::transform(factor_name_lower.begin(), factor_name_lower.end(),
                           factor_name_lower.begin(), ::tolower);

            // MAHIMA: Special handling for income and physical activity
            //  These are stored in member variables, not just in risk_factors map
            //  Note: Factor name in expected table is "income" (canonical)
            //        Factor name in expected table is "PhysicalActivity" (canonical), but
            //        internally stored as physical_activity
            if (factor_name_lower == "income") {
                // Get current value from risk_factors["income"] or person.income_continuous (for
                // continuous model)
                double current_value = 0.0;
                if (person.risk_factors.contains(factor)) {
                    current_value = person.risk_factors.at(factor);
                } else if (person.income_continuous > 0.0) {
                    // Fallback: use member variable if risk_factors doesn't have it (continuous
                    // model)
                    current_value = person.income_continuous;
                }

                // Apply adjustment: new_value = current_value + delta
                double adjusted_value = current_value + delta;

                // Do not clamp income to a range: income is continuous and should match
                // factors-mean scale (e.g. ~621 for age 0). Using another factor's range (e.g.
                // 0–42.7) would cap everyone at 42.7 and collapse quartiles, leaving only
                // low/lowermiddle. (Ranges passed in are for base risk factors; income uses a
                // different scale.)

                // Update risk_factors["income"] (canonical name for mapping/output)
                person.risk_factors[factor] = adjusted_value;
                // Also update person.income_continuous if it was previously set (continuous model)
                if (person.income_continuous > 0.0) {
                    person.income_continuous = adjusted_value;
                }
            } else if (factor_name == "PhysicalActivity") {
                // Factor name "PhysicalActivity" from expected table maps to
                // person.physical_activity internally Get current value from member variable
                double current_value = person.physical_activity;
                const core::Identifier physical_activity_id("PhysicalActivity");
                if (person.risk_factors.contains(physical_activity_id)) {
                    // Use risk_factors value if it exists (may be more up-to-date)
                    current_value = person.risk_factors.at(physical_activity_id);
                }

                // Apply adjustment: new_value = current_value + delta
                double adjusted_value = current_value + delta;

                // Clamp value to an optionally specified range
                if (ranges.has_value() && i < ranges.value().get().size()) {
                    const auto &range = ranges.value().get()[i];
                    adjusted_value = range.clamp(adjusted_value);
                }

                // Update both member variable and risk_factors map for consistency
                person.physical_activity = adjusted_value;
                person.risk_factors[physical_activity_id] = adjusted_value;
            } else {
                // Regular risk factor: stored only in risk_factors map
                // Get current value from risk_factors
                double value = person.risk_factors.at(factor) + delta;

                // Clamp value to an optionally specified range
                if (ranges.has_value() && i < ranges.value().get().size()) {
                    const auto &range = ranges.value().get()[i];
                    value = range.clamp(value);
                }

                // Set the adjusted value
                person.risk_factors.at(factor) = value;
            }
        }
    });

    // Baseline scenario: send adjustments to intervention scenario.
    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<RiskFactorAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(adjustments)));
    }
    // Print which risk factors are being adjusted to their factors mean
    /* std::cout << "\n=== RISK FACTORS BEING ADJUSTED TO FACTORS MEAN ===";
    std::cout << "\nTotal factors: " << factors.size();
    for (size_t i = 0; i < factors.size(); i++) {
        std::cout << "\n  " << (i + 1) << ". " << factors[i].to_string();
    }
    std::cout << "\n===================================================\n";*/
}

int RiskFactorAdjustableModel::get_trend_steps(const core::Identifier &factor) const {
    if (trend_steps_) {
        return trend_steps_->at(factor);
    }
    return 0; // Default to no trend steps if trend data is not available
}

void RiskFactorAdjustableModel::set_logistic_factors(
    const std::unordered_set<core::Identifier> &logistic_factors) {
    logistic_factors_ = logistic_factors;
}

RiskFactorSexAgeTable
RiskFactorAdjustableModel::calculate_adjustments(RuntimeContext &context,
                                                 const std::vector<core::Identifier> &factors,
                                                 OptionalRanges ranges, bool apply_trend) const {
    auto age_range = context.age_range();
    auto age_count = age_range.upper() + 1;

    // Compute simulated means.
    auto simulated_means =
        calculate_simulated_mean(context.population(), age_range, factors, logistic_factors_);

    // Compute adjustments.
    auto adjustments = RiskFactorSexAgeTable{};
    for (const auto &[sex, simulated_means_by_sex] : simulated_means) {
        for (size_t i = 0; i < factors.size(); i++) {
            const core::Identifier &factor = factors[i];

            // Check if this factor exists in the expected table
            if (!expected_->contains(sex, factor)) {
                std::cout << "\nWARNING - Factor " << factor.to_string()
                          << " not found in expected table for gender "
                          << (sex == core::Gender::male ? "Male" : "Female") << " - skipping";
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

                // Debug for income: expected vs simulated_mean vs delta (age 0 and 1 for both
                // sexes)
                if (factor.to_string() == "income" &&
                    (age == age_range.lower() ||
                     (age == age_range.lower() + 1 && age <= age_range.upper()))) {
                    std::cout << "\n[INCOME ADJUST] "
                              << (sex == core::Gender::male ? "male" : "female") << " age" << age
                              << " expected=" << expect << " simulated_mean=" << sim_mean
                              << " delta=" << delta;
                    if (expect < 100.0 && sim_mean > 200.0) {
                        static bool warned = false;
                        if (!warned) {
                            std::cout << "\n[INCOME] WARNING: factors mean 'income' column has "
                                         "expected="
                                      << expect << " but simulated mean is ~" << sim_mean
                                      << ". For continuous income use values in the same scale "
                                         "(e.g. ~600 for age 0) in factorsmean_male/female CSV, "
                                         "not category (1-4) or another variable.";
                            warned = true;
                        }
                    }
                }
            }
        }
    }

    return adjustments;
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

            std::optional<double> opt_value = get_factor_value_for_person(person, factor);
            if (!opt_value.has_value()) {
                continue;
            }
            const double value = *opt_value;
            total_counts[factor]++;
            if (!should_include_in_simulated_mean(value, factor, logistic_factors)) {
                excluded_counts[factor]++;
                continue;
            }
            moments.at(person.gender, factor).at(person.age).append(value);
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

    print_excluded_summary_once(factors, logistic_factors, excluded_counts, total_counts);

    return means;
}

RiskFactorAdjustableModelDefinition::RiskFactorAdjustableModelDefinition(
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps, TrendType trend_type)
    : expected_{std::move(expected)}, expected_trend_{std::move(expected_trend)},
      trend_steps_{std::move(trend_steps)}, trend_type_{trend_type} {

    if (expected_->empty()) {
        throw core::HgpsException("Risk factor expected value mapping is empty");
    }

    // Only validate trend data if trends are actually being used
    if (trend_type_ != TrendType::Null) {
        if (expected_trend_->empty()) {
            throw core::HgpsException("Risk factor expected trend mapping is empty");
        }
        if (trend_steps_->empty()) {
            throw core::HgpsException("Risk factor trend steps mapping is empty");
        }
    }
}

} // namespace hgps
