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

RiskfactorAdjustmentModel::RiskfactorAdjustmentModel(BaselineAdjustment &adjustments)
    : adjustments_{adjustments} {}

void RiskfactorAdjustmentModel::Apply(RuntimeContext &context) {
    auto &pop = context.population();
    auto coefficients = get_adjustment_coefficients(context);
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

RiskFactorSexAgeTable
RiskfactorAdjustmentModel::get_adjustment_coefficients(RuntimeContext &context) const {
    if (context.scenario().type() == ScenarioType::baseline) {
        return calculate_adjustment_coefficients(context);
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
RiskfactorAdjustmentModel::calculate_adjustment_coefficients(RuntimeContext &context) const {
    const auto &age_range = context.age_range();
    auto max_age = age_range.upper() + 1;
    auto coefficients =
        std::unordered_map<core::Gender,
                           std::unordered_map<core::Identifier, std::vector<double>>>{};

    auto simulated_means = calculate_simulated_mean(context.population(), age_range);
    auto &baseline_means = adjustments_.get().values;
    for (auto &gender : simulated_means) {
        coefficients.emplace(gender.first,
                             std::unordered_map<core::Identifier, std::vector<double>>{});
        for (auto &factor : gender.second) {
            coefficients.at(gender.first).emplace(factor.first, std::vector<double>(max_age, 0.0));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                auto coeff_value =
                    baseline_means.at(gender.first, factor.first).at(age) - factor.second.at(age);
                if (!std::isnan(coeff_value)) {
                    coefficients.at(gender.first).at(factor.first).at(age) = coeff_value;
                }
            }
        }
    }

    return RiskFactorSexAgeTable{std::move(coefficients)};
}

RiskFactorSexAgeTable
RiskfactorAdjustmentModel::calculate_simulated_mean(Population &population,
                                                    const core::IntegerInterval &age_range) {
    auto max_age = age_range.upper() + 1;
    auto moments =
        std::unordered_map<core::Gender,
                           std::unordered_map<core::Identifier, std::vector<FirstMoment>>>{};
    moments.emplace(core::Gender::male,
                    std::unordered_map<core::Identifier, std::vector<FirstMoment>>{});
    moments.emplace(core::Gender::female,
                    std::unordered_map<core::Identifier, std::vector<FirstMoment>>{});

    for (const auto &entity : population) {
        if (!entity.is_active()) {
            continue;
        }

        auto &table = moments.at(entity.gender);
        for (const auto &factor : entity.risk_factors) {
            if (!table.contains(factor.first)) {
                table.emplace(factor.first, std::vector<FirstMoment>(max_age));
            }

            table.at(factor.first).at(entity.age).append(factor.second);
        }
    }

    auto means = std::unordered_map<core::Gender,
                                    std::unordered_map<core::Identifier, std::vector<double>>>{};
    for (auto &gender : moments) {
        means.emplace(gender.first, std::unordered_map<core::Identifier, std::vector<double>>{});
        for (auto &factor : gender.second) {
            means.at(gender.first).emplace(factor.first, std::vector<double>(max_age, 0.0));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                means.at(gender.first).at(factor.first).at(age) = factor.second.at(age).mean();
            }
        }
    }

    return RiskFactorSexAgeTable{std::move(means)};
}

} // namespace hgps
