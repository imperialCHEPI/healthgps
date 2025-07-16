#include "HealthGPS.Core/exception.h"

#include "risk_factor_adjustable_model.h"
#include "sync_message.h"

#include <iostream>
#include <oneapi/tbb/parallel_for_each.h>
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
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Starting
    // (apply_trend=" << (apply_trend ? "true" : "false") << ")" << std::endl;
    RiskFactorSexAgeTable adjustments;

    // Flag to track if ranges are applied at least once
    // bool any_ranges_applied = false;

    // Baseline scenario: compute adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        // std::cout << "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Calculating
        // adjustments for baseline";
        adjustments = calculate_adjustments(context, factors, ranges, apply_trend);
        // std::cout<< "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Adjustments
        // calculated";
    }

    // Intervention scenario: receive adjustments from baseline scenario.
    else {
        // std::cout << "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Receiving
        // adjustments for intervention";
        auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
        while (!message.has_value()) {
            message = context.scenario().channel().try_receive(context.sync_timeout_millis());
        }

        // Keep trying until we get a message of the correct type
        auto *messagePrt = dynamic_cast<RiskFactorAdjustmentMessage *>(message.value().get());
        while (!messagePrt) {
            message = context.scenario().channel().try_receive(context.sync_timeout_millis());
            while (!message.has_value()) {
                message = context.scenario().channel().try_receive(context.sync_timeout_millis());
            }
            messagePrt = dynamic_cast<RiskFactorAdjustmentMessage *>(message.value().get());
        }

        adjustments = messagePrt->data();
        // std::cout<< "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Adjustments
        // received";
    }

    // All scenarios: apply adjustments to population.
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Applying adjustments
    // to population";
    auto &pop = context.population();
    tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }

        for (size_t factor_index = 0; factor_index < factors.size(); factor_index++) {
            const auto &factor = factors[factor_index];

            // MAHIMA: Skip adjustment for risk factors that are currently 0
            // This preserves zero values set by logistic regression (two-stage modeling)
            double original_value = person.risk_factors.at(factor);
            if (original_value == 0.0) {
                continue; // Don't adjust zero risk factors
            }

            double delta = adjustments.at(person.gender, factor).at(person.age);
            double value = original_value + delta;

            // Check for NaN values and fix them
            if (std::isnan(value)) {
                std::cout << "\nWARNING: NaN value detected during risk factor adjustment for "
                          << factor.to_string() << " (Person ID: " << person.id() << ", Gender: "
                          << (person.gender == core::Gender::male ? "Male" : "Female")
                          << ", Age: " << person.age << ", Original value: " << original_value
                          << ", Delta: " << delta << ")";

                // Use original value as fallback
                value = original_value;
            }

            // Apply range clamping if ranges are provided
            if (ranges.has_value()) {
                // Make sure we're using the right index for the range
                value = ranges.value().get().at(factor_index).clamp(value);
                // any_ranges_applied = true; // Mark that ranges were applied at least once
            }

            // Set the adjusted value with any additional validation
            person.set_risk_factor(context, factor, value);
        }

        // Special handling for demographic attributes- Mahima
        // Adjusting income and physical activity to factors mean
        // Map income_continuous to Income
        if (std::find(factors.begin(), factors.end(), "Income"_id) != factors.end()) {
            double delta = adjustments.at(person.gender, "Income"_id).at(person.age);
            person.income_continuous += delta;
        }

        // Map physical_activity to PhysicalActivity
        if (std::find(factors.begin(), factors.end(), "PhysicalActivity"_id) != factors.end()) {
            double delta = adjustments.at(person.gender, "PhysicalActivity"_id).at(person.age);
            person.physical_activity += delta;
        }
    });
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Adjustments applied";

    // Print single debug message about range application
    // std::cout << "\nRANGE DEBUG: " << (any_ranges_applied ? "Ranges were applied for risk factor
    // adjustment": "NO ranges were applied during adjustment");

    // Baseline scenario: send adjustments to intervention scenario.
    if (context.scenario().type() == ScenarioType::baseline) {
        // std::cout << "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Sending
        // adjustments to intervention";
        context.scenario().channel().send(std::make_unique<RiskFactorAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(adjustments)));
        // std::cout << "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Adjustments
        // sent";
    }

    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::adjust_risk_factors - Completed" <<
    // std::endl;
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
    // std::cout<< "\nDEBUG: RiskFactorAdjustableModel::calculate_adjustments - Calculating
    // simulated means";
    auto simulated_means = calculate_simulated_mean(context.population(), age_range, factors);
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_adjustments - Simulated means
    // calculated";

    // Compute adjustments.
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_adjustments - Computing
    // adjustments";
    auto adjustments = RiskFactorSexAgeTable{};
    for (const auto &[sex, simulated_means_by_sex] : simulated_means) {
        for (size_t i = 0; i < factors.size(); i++) {
            const core::Identifier &factor = factors[i];

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
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_simulated_mean - Starting";
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

            // Check if the risk factor exists for this person
            if (!person.risk_factors.contains(factor)) {
                std::cout << "\nDEBUG: WARNING - Person #" << person.id()
                          << " is missing risk factor: " << factor.to_string() << "\n";
                continue;
            }

            double value = person.risk_factors.at(factor);
            moments.at(person.gender, factor).at(person.age).append(value);
        }

        /*if (processed_count % 1000 == 0) {
            std::cout << "\nDEBUG: Processed " << processed_count << " people for first moments"
                      << std::endl;
        }*/
    }

    // Compute means.
    // std::cout << "\nDEBUG: RiskFactorAdjustableModel::calculate_simulated_mean - Computing
    // means";
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
