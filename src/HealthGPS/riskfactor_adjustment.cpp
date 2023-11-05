#include "riskfactor_adjustment.h"
#include "HealthGPS.Core/thread_util.h"
#include "sync_message.h"

#include <cmath>
#include <memory>

namespace { // anonymous namespace

/// @brief Defines the baseline risk factors adjustment synchronisation message
using BaselineAdjustmentMessage = hgps::SyncDataMessage<hgps::RiskFactorSexAgeTable>;

/// @brief Defines the first statistical moment type
struct FirstMoment {
    /// @brief Determine whether the moment is empty
    /// @return true if the moment is empty; otherwise, false.
    bool empty() const noexcept { return count_ < 1; }
    /// @brief Gets the number of values added to the moment
    /// @return Number of values
    int count() const noexcept { return count_; }
    /// @brief Gets the values sum
    /// @return Sum value
    double sum() const noexcept { return sum_; }

    /// @brief Gets the values mean
    /// @return Mean value
    double mean() const noexcept {
        if (count_ > 0) {
            return sum_ / count_;
        }

        return 0.0;
    }

    /// @brief Appends a value to the moment
    /// @param value The new value
    void append(double value) noexcept {
        count_++;
        sum_ += value;
    }

    /// @brief Clear the moment
    void clear() noexcept {
        count_ = 0;
        sum_ = 0.0;
    }

    /// @brief Compare FirstMoment instances
    /// @param other The other instance to compare
    /// @return The comparison result
    auto operator<=>(const FirstMoment &other) const = default;

  private:
    int count_{};
    double sum_{};
};

} // anonymous namespace

namespace hgps {

RiskfactorAdjustmentModel::RiskfactorAdjustmentModel(BaselineAdjustment &risk_factor_expected)
    : risk_factor_expected_{risk_factor_expected} {}

void RiskfactorAdjustmentModel::Apply(RuntimeContext &context) {
    auto &pop = context.population();
    auto coefficients = get_adjustments(context);
    std::for_each(core::execution_policy, pop.begin(), pop.end(), [&](auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        auto &table = coefficients.at(entity.gender);
        for (auto &factor : table) {
            auto current_value = entity.get_risk_factor_value(factor.first);
            auto adjustment = factor.second.at(entity.age);
            entity.risk_factors.at(factor.first) = current_value + adjustment;
        }
    });

    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<BaselineAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(coefficients)));
    }
}

RiskFactorSexAgeTable RiskfactorAdjustmentModel::get_adjustments(RuntimeContext &context) const {
    if (context.scenario().type() == ScenarioType::baseline) {
        return calculate_adjustments(context);
    }

    // Receive message with timeout
    auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
    if (!message.has_value()) {
        throw std::runtime_error(
            "Simulation out of sync, receive baseline adjustments message has timed out");
    }

    auto &basePtr = message.value();
    auto *messagePrt = dynamic_cast<BaselineAdjustmentMessage *>(basePtr.get());
    if (!messagePrt) {
        throw std::runtime_error(
            "Simulation out of sync, failed to receive a baseline adjustments message");
    }

    return messagePrt->data();
}

RiskFactorSexAgeTable
RiskfactorAdjustmentModel::calculate_adjustments(RuntimeContext &context) const {
    const auto &age_range = context.age_range();
    auto max_age = age_range.upper() + 1;

    // Compute simulated means.
    auto simulated_means = calculate_simulated_mean(context);
    auto &baseline_means = risk_factor_expected_.get().values;

    // Compute adjustments.
    auto adjustments = RiskFactorSexAgeTable{};
    for (auto &gender : simulated_means) {
        for (auto &factor : gender.second) {
            adjustments.emplace(gender.first, factor.first, std::vector<double>(max_age, 0.0));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                const double expected = baseline_means.at(gender.first, factor.first).at(age);
                const double simulated = factor.second.at(age);
                const double delta = expected - simulated;
                if (!std::isnan(delta)) {
                    adjustments.at(gender.first, factor.first).at(age) = delta;
                }
            }
        }
    }

    return adjustments;
}

RiskFactorSexAgeTable RiskfactorAdjustmentModel::calculate_simulated_mean(RuntimeContext &context) {
    auto age_range = context.age_range();
    auto max_age = age_range.upper() + 1;

    // Compute first moments.
    auto moments = UnorderedMap2d<core::Gender, core::Identifier, std::vector<FirstMoment>>{};
    for (const auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        for (const auto &factor : person.risk_factors) {
            if (!moments.contains(person.gender, factor.first)) {
                moments.emplace(person.gender, factor.first, std::vector<FirstMoment>(max_age));
            }
            moments.at(person.gender, factor.first).at(person.age).append(factor.second);
        }
    }

    // Compute means.
    auto means = RiskFactorSexAgeTable{};
    for (auto &gender : moments) {
        for (auto &factor : gender.second) {
            means.emplace(gender.first, factor.first, std::vector<double>(max_age, 0.0));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                means.at(gender.first, factor.first).at(age) = factor.second.at(age).mean();
            }
        }
    }

    return means;
}

} // namespace hgps
