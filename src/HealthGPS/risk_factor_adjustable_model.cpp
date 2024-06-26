#include "HealthGPS.Core/exception.h"

#include "risk_factor_adjustable_model.h"
#include "sync_message.h"

#include <oneapi/tbb/parallel_for_each.h>

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
    const RiskFactorSexAgeTable &risk_factor_expected)
    : risk_factor_expected_{risk_factor_expected} {}

const RiskFactorSexAgeTable &RiskFactorAdjustableModel::get_risk_factor_expected() const noexcept {
    return risk_factor_expected_;
}

void RiskFactorAdjustableModel::adjust_risk_factors(RuntimeContext &context,
                                                    const std::vector<core::Identifier> &factors,
                                                    OptionalRanges ranges) const {
    RiskFactorSexAgeTable adjustments;

    // Baseline scenatio: compute adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        adjustments = calculate_adjustments(context, factors);
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
    tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }

        for (size_t i = 0; i < factors.size(); i++) {
            double delta = adjustments.at(person.gender, factors[i]).at(person.age);
            double value = person.risk_factors.at(factors[i]) + delta;

            // Clamp value to an optionally specified range.
            if (ranges.has_value()) {
                const auto &range = ranges.value().get()[i];
                value = range.clamp(value);
            }

            // Set the adjusted value.
            person.risk_factors.at(factors[i]) = value;
        }
    });

    // Baseline scenario: send adjustments to intervention scenario.
    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<RiskFactorAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(adjustments)));
    }
}

RiskFactorSexAgeTable RiskFactorAdjustableModel::calculate_adjustments(
    RuntimeContext &context, const std::vector<core::Identifier> &factors) const {
    auto age_range = context.age_range();
    auto age_count = age_range.upper() + 1;

    // Compute simulated means.
    auto simulated_means = calculate_simulated_mean(context.population(), age_range, factors);

    // Compute adjustments.
    auto adjustments = RiskFactorSexAgeTable{};
    for (const auto &[sex, simulated_means_by_sex] : simulated_means) {
        for (const auto &factor : factors) {
            adjustments.emplace(sex, factor, std::vector<double>(age_count));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                double expect = risk_factor_expected_.at(sex, factor).at(age);
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
            double value = person.risk_factors.at(factor);
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
    RiskFactorSexAgeTable risk_factor_expected)
    : RiskFactorModelDefinition{}, risk_factor_expected_{std::move(risk_factor_expected)} {

    if (risk_factor_expected_.empty()) {
        throw core::HgpsException("Risk factor expected value mapping is empty");
    }
}

const RiskFactorSexAgeTable &
RiskFactorAdjustableModelDefinition::get_risk_factor_expected() const noexcept {
    return risk_factor_expected_;
}

} // namespace hgps
