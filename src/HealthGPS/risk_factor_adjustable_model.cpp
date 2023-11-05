#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/thread_util.h"

#include "risk_factor_adjustable_model.h"
#include "sync_message.h"

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

    double mean() const noexcept { return (count_ > 0) ? (sum_ / count_) : 0.0; }

  private:
    int count_{};
    double sum_{};
};

} // anonymous namespace

namespace hgps {

RiskFactorAdjustableModel::RiskFactorAdjustableModel(
    const RiskFactorSexAgeTable &risk_factor_expected)
    : risk_factor_expected_{risk_factor_expected} {

    if (risk_factor_expected_.empty()) {
        throw core::HgpsException("Risk factor expected value mapping is empty");
    }
}

const RiskFactorSexAgeTable &RiskFactorAdjustableModel::get_risk_factor_expected() const noexcept {
    return risk_factor_expected_;
}

void RiskFactorAdjustableModel::adjust_risk_factors(
    RuntimeContext &context, const std::unordered_set<core::Identifier> &risk_factor_names) const {
    RiskFactorSexAgeTable adjustments = get_adjustments(context);

    auto &pop = context.population();
    std::for_each(core::execution_policy, pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }

        for (auto &factor_key : risk_factor_names) {
            const double delta = adjustments.at(person.gender, factor_key).at(person.age);
            person.risk_factors.at(factor_key) += delta;
        }
    });

    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<RiskFactorAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(adjustments)));
    }
}

RiskFactorSexAgeTable RiskFactorAdjustableModel::get_adjustments(RuntimeContext &context) const {
    if (context.scenario().type() == ScenarioType::baseline) {
        return calculate_adjustments(context);
    }

    // Receive message with timeout
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

    return messagePrt->data();
}

RiskFactorSexAgeTable
RiskFactorAdjustableModel::calculate_adjustments(RuntimeContext &context) const {
    const auto &age_range = context.age_range();
    auto max_age = age_range.upper() + 1;

    // Compute simulated means.
    auto simulated_means = calculate_simulated_mean(context);

    // Compute adjustments.
    auto adjustments = RiskFactorSexAgeTable{};
    for (auto &gender : simulated_means) {
        for (auto &factor : gender.second) {
            adjustments.emplace(gender.first, factor.first, std::vector<double>(max_age, 0.0));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                const double expected =
                    risk_factor_expected_.at(gender.first, factor.first).at(age);
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

RiskFactorSexAgeTable RiskFactorAdjustableModel::calculate_simulated_mean(RuntimeContext &context) {
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
