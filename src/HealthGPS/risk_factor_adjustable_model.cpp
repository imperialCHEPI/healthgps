#include "HealthGPS.Core/exception.h"

#include "risk_factor_adjustable_model.h"

#include "sync_message.h"
#include <fmt/core.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
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
    const std::string &key = factor.to_string();
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
bool should_include_in_simulated_mean(
    double value, const hgps::core::Identifier &factor,
    const std::unordered_set<hgps::core::Identifier> &logistic_factors) {
    if (!logistic_factors.contains(factor)) {
        return true;
    }
    return value != 0;
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RiskFactorAdjustableModel::adjust_risk_factors(
    RuntimeContext &context, const std::vector<core::Identifier> &factors, OptionalRanges ranges,
    bool apply_trend, const RiskFactorSexAgeTable *expected_override,
    std::optional<std::size_t> income_stratum_filter,
    std::vector<IncomeStratumAdjustmentExampleRow> *debug_example_rows) const {
    RiskFactorSexAgeTable adjustments;
    std::vector<IncomeStratumAdjustmentExampleRow> sampled_rows;
    const bool collect_debug_rows =
        debug_example_rows != nullptr && income_stratum_filter.has_value() &&
        expected_override != nullptr && context.scenario().type() == ScenarioType::baseline;
    const auto sex_to_label = [](core::Gender sex) -> std::string_view {
        return sex == core::Gender::male ? "male" : "female";
    };
    const auto equal_factor_name = [](std::string_view lhs, std::string_view rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
                std::tolower(static_cast<unsigned char>(rhs[i]))) {
                return false;
            }
        }
        return true;
    };

    // Baseline scenatio: compute adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        adjustments = calculate_adjustments(context, factors, ranges, apply_trend,
                                            expected_override, income_stratum_filter,
                                            collect_debug_rows ? &sampled_rows : nullptr);
        if (collect_debug_rows) {
            for (auto &row : sampled_rows) {
                const core::Identifier factor_id(row.factor);
                double best_hash_score = std::numeric_limits<double>::infinity();
                for (const auto &person : context.population()) {
                    if (!person.is_active()) {
                        continue;
                    }
                    if (income_stratum_filter.has_value() &&
                        (!person.has_income_adjustment_stratum ||
                         person.income_adjustment_stratum != income_stratum_filter.value())) {
                        continue;
                    }
                    if (row.age < 0) {
                        continue;
                    }
                    const auto row_age = static_cast<unsigned int>(row.age);
                    if (person.age != row_age) {
                        continue;
                    }
                    if (!equal_factor_name(sex_to_label(person.gender), row.sex)) {
                        continue;
                    }
                    double current_value = 0.0;
                    bool has_value = false;
                    if (equal_factor_name(row.factor, "income")) {
                        if (person.risk_factors.contains(factor_id)) {
                            current_value = person.risk_factors.at(factor_id);
                            has_value = true;
                        } else if (person.income_continuous > 0.0) {
                            current_value = person.income_continuous;
                            has_value = true;
                        }
                    } else if (equal_factor_name(row.factor, "PhysicalActivity")) {
                        const core::Identifier pa_id("PhysicalActivity");
                        current_value = person.physical_activity;
                        if (person.risk_factors.contains(pa_id)) {
                            current_value = person.risk_factors.at(pa_id);
                        }
                        has_value = true;
                    } else if (person.risk_factors.contains(factor_id)) {
                        current_value = person.risk_factors.at(factor_id);
                        has_value = true;
                    }
                    if (!has_value) {
                        continue;
                    }
                    // MAHIMA: These numbers are carefully chosen hash‑mixing constants that turn
                    // simple inputs (ID, age, bucket) into a well‑distributed, deterministic,
                    // random‑looking score without bias or collisions.
                    const auto hash_seed =
                        (static_cast<std::uint64_t>(person.id()) * 1315423911ULL) ^
                        (static_cast<std::uint64_t>(row.age) * 2654435761ULL) ^
                        (static_cast<std::uint64_t>(row.bucket + 1u) * 97531ULL);
                    const auto score = static_cast<double>(hash_seed % 1000003ULL);
                    if (score < best_hash_score) {
                        best_hash_score = score;
                        row.person_id = person.id();
                        row.current_value = current_value;
                        row.after_delta_value = current_value + row.delta;
                    }
                }
            }
        }
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
    tbb::parallel_for_each(
        pop.begin(), pop.end(),
        [&](auto &person) { // NOLINT(readability-function-cognitive-complexity)
            if (!person.is_active()) {
                return;
            }
            if (income_stratum_filter.has_value() &&
                (!person.has_income_adjustment_stratum ||
                 person.income_adjustment_stratum != income_stratum_filter.value())) {
                return;
            }

            for (size_t i = 0; i < factors.size(); i++) {
                const core::Identifier &factor = factors[i];
                // MAHIMA: Defensive guard for sparse/partial adjustment tables.
                // In stratum mode, a factor/sex bucket may be missing in rare edge cases
                // (e.g. empty stratum after filtering or incomplete expected overrides).
                // Skip safely instead of risking invalid memory access while applying deltas.
                if (!adjustments.contains(person.gender, factor)) {
                    continue;
                }
                const auto &delta_by_age = adjustments.at(person.gender, factor);
                if (static_cast<std::size_t>(person.age) >= delta_by_age.size()) {
                    continue;
                }
                double delta = delta_by_age.at(static_cast<std::size_t>(person.age));
                const std::string &factor_name = factor.to_string();
                std::string factor_name_lower = factor_name;
                std::ranges::transform(factor_name_lower, factor_name_lower.begin(), ::tolower);

                // MAHIMA: Special handling for income and physical activity
                //  These are stored in member variables, not just in risk_factors map
                //  Note: Factor name in expected table is "income" (canonical)
                //        Factor name in expected table is "PhysicalActivity" (canonical), but
                //        internally stored as physical_activity
                if (factor_name_lower == "income") {
                    // Get current value from risk_factors["income"] or person.income_continuous
                    // (for continuous model)
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
                    // MAHIMA: We make sure that min/max of income models is used but we do not
                    // clamp Do not clamp income to a range: income is continuous and should match
                    // factors-mean scale (e.g. ~621 for age 0). Using another factor's range (e.g.
                    // 0–42.7) would cap everyone at 42.7 and collapse quartiles, leaving only
                    // low/lowermiddle. (Ranges passed in are for base risk factors; income uses a
                    // different scale.)
                    // The problem is that in income module csv file, income ranges are diffrent
                    // from the fcators mean range. In the csv, the range is 25-2379 wheras in
                    // factors mean, the range is 769. So majority of the people will get clamped.

                    // Update risk_factors["income"] (canonical name for mapping/output)
                    person.risk_factors[factor] = adjusted_value;
                    // Also update person.income_continuous if it was previously set (continuous
                    // model)
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
                    double current_value = person.risk_factors.at(factor);
                    double value = current_value + delta;

                    // Clamp value to an optionally specified range
                    if (ranges.has_value() && i < ranges.value().get().size()) {
                        const auto &range = ranges.value().get()[i];
                        value = range.clamp(value);
                    }

                    // Set the adjusted value to the risk factor
                    person.risk_factors.at(factor) = value;
                }
            }
        });

    if (collect_debug_rows && !sampled_rows.empty()) {
        for (auto &row : sampled_rows) {
            if (row.person_id == 0u) {
                continue;
            }
            const auto it_person =
                std::find_if(context.population().begin(), context.population().end(),
                             [&](const Person &p) { return p.id() == row.person_id; });
            if (it_person == context.population().end()) {
                continue;
            }
            const auto &person = *it_person;
            const core::Identifier factor_id(row.factor);
            if (equal_factor_name(row.factor, "income")) {
                if (person.risk_factors.contains(factor_id)) {
                    row.final_value = person.risk_factors.at(factor_id);
                } else {
                    row.final_value = row.after_delta_value;
                }
            } else if (equal_factor_name(row.factor, "PhysicalActivity")) {
                const core::Identifier pa_id("PhysicalActivity");
                row.final_value = person.physical_activity;
                if (person.risk_factors.contains(pa_id)) {
                    row.final_value = person.risk_factors.at(pa_id);
                }
            } else if (person.risk_factors.contains(factor_id)) {
                row.final_value = person.risk_factors.at(factor_id);
            } else {
                row.final_value = row.after_delta_value;
            }
        }

        for (auto &row : sampled_rows) {
            if (row.person_id == 0u) {
                continue;
            }
            debug_example_rows->push_back(std::move(row));
        }
    }

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

// NOLINTBEGIN(readability-function-cognitive-complexity)
RiskFactorSexAgeTable RiskFactorAdjustableModel::calculate_adjustments(
    RuntimeContext &context, const std::vector<core::Identifier> &factors, OptionalRanges ranges,
    bool apply_trend, const RiskFactorSexAgeTable *expected_override,
    std::optional<std::size_t> income_stratum_filter,
    std::vector<IncomeStratumAdjustmentExampleRow> *debug_delta_rows) const {
    auto age_range = context.age_range();
    auto age_count = age_range.upper() + 1;

    const auto *expected_table = expected_override != nullptr ? expected_override : expected_.get();

    // Compute simulated means.
    auto simulated_means = calculate_simulated_mean(context.population(), age_range, factors,
                                                    logistic_factors_, income_stratum_filter);

    // Compute adjustments.
    auto adjustments = RiskFactorSexAgeTable{};
    std::vector<std::pair<std::uint64_t, IncomeStratumAdjustmentExampleRow>> debug_candidates;
    debug_candidates.reserve(32);
    constexpr std::size_t max_debug_rows_per_call = 8u;
    for (const auto &[sex, simulated_means_by_sex] : simulated_means) {
        for (size_t i = 0; i < factors.size(); i++) {
            const core::Identifier &factor = factors[i];

            // Check if this factor exists in the expected table
            if (!expected_table->contains(sex, factor)) {
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
                double expect = 0.0;
                if (!expected_table->contains(sex, factor)) {
                    continue;
                }
                expect = expected_table->at(sex, factor).at(age);
                if (apply_trend && trend_type_ != TrendType::Null) {
                    int elapsed_time = context.time_now() - context.start_time();
                    switch (trend_type_) {
                    case TrendType::Null:
                        break;
                    case TrendType::UPFTrend: {
                        if (expected_trend_) {
                            int t = std::min(elapsed_time, get_trend_steps(factor));
                            expect *= pow(expected_trend_->at(factor), t);
                        }
                        break;
                    }
                    case TrendType::IncomeTrend: {
                        if (elapsed_time > 0 && expected_income_trend_ &&
                            expected_income_trend_decay_factors_) {
                            double decay_factor = expected_income_trend_decay_factors_->at(factor);
                            double expected_income_trend = expected_income_trend_->at(factor);
                            expect *= expected_income_trend * exp(decay_factor * elapsed_time);
                        }
                        break;
                    }
                    }
                }
                if (range.has_value()) {
                    expect = range.value().get().clamp(expect);
                }
                double sim_mean = simulated_means_by_sex.at(factor).at(age);

                // Delta should remain zero if simulated mean is NaN.
                double delta = 0.0;

                // Else, delta is the distance from expected value.
                if (!std::isnan(sim_mean)) {
                    delta = expect - sim_mean;
                }

                adjustments.at(sex, factor).at(age) = delta;
                if (debug_delta_rows != nullptr && income_stratum_filter.has_value() &&
                    expected_override != nullptr &&
                    context.scenario().type() == ScenarioType::baseline && !std::isnan(sim_mean)) {
                    IncomeStratumAdjustmentExampleRow row;
                    row.bucket = income_stratum_filter.value();
                    row.factor = factor.to_string();
                    row.sex = sex == core::Gender::male ? "male" : "female";
                    row.age = age;
                    row.expected_value = expect;
                    row.simulated_mean = sim_mean;
                    row.delta = delta;

                    std::uint64_t score =
                        static_cast<std::uint64_t>(row.bucket + 1u) * 1099511628211ULL;
                    score ^= static_cast<std::uint64_t>(age + 4099);
                    score ^= static_cast<std::uint64_t>(row.factor.size()) * 1469598103934665603ULL;
                    score ^= static_cast<std::uint64_t>(row.sex == "male" ? 131u : 239u);
                    for (char c : row.factor) {
                        score ^= static_cast<std::uint64_t>(static_cast<unsigned char>(std::tolower(
                                                                static_cast<unsigned char>(c))) +
                                                            1u);
                        score *= 1099511628211ULL;
                    }
                    debug_candidates.emplace_back(score, std::move(row));
                }
            }
        }
    }

    if (debug_delta_rows != nullptr && !debug_candidates.empty()) {
        std::ranges::sort(debug_candidates, [](const auto &lhs, const auto &rhs) {
            if (lhs.first != rhs.first) {
                return lhs.first < rhs.first;
            }
            if (lhs.second.factor != rhs.second.factor) {
                return lhs.second.factor < rhs.second.factor;
            }
            if (lhs.second.sex != rhs.second.sex) {
                return lhs.second.sex < rhs.second.sex;
            }
            return lhs.second.age < rhs.second.age;
        });
        const std::size_t sample_count = std::min(max_debug_rows_per_call, debug_candidates.size());
        for (std::size_t i = 0; i < sample_count; ++i) {
            debug_delta_rows->push_back(std::move(debug_candidates[i].second));
        }
    }

    return adjustments;
}
// NOLINTEND(readability-function-cognitive-complexity)

RiskFactorSexAgeTable RiskFactorAdjustableModel::calculate_simulated_mean(
    Population &population, const core::IntegerInterval age_range,
    const std::vector<core::Identifier> &factors,
    const std::unordered_set<core::Identifier> &logistic_factors,
    std::optional<std::size_t> income_stratum_filter) {
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
        if (income_stratum_filter.has_value() &&
            (!person.has_income_adjustment_stratum ||
             person.income_adjustment_stratum != income_stratum_filter.value())) {
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
