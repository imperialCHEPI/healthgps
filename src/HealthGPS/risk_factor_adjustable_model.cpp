#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/thread_util.h"

#include "risk_factor_adjustable_model.h"
#include "sync_message.h"

namespace { // anonymous namespace

/// @brief Defines the baseline risk factors adjustment synchronisation message
using RiskFactorAdjustmentMessage = hgps::SyncDataMessage<hgps::SexAgeTable>;

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

RiskFactorAdjustableModel::RiskFactorAdjustableModel(const BaselineAdjustment &risk_factor_expected)
    : risk_factor_expected_{risk_factor_expected} {

    if (risk_factor_expected_.values.empty()) {
        throw core::HgpsException("Risk factor means mapping is empty");
    }
}

const BaselineAdjustment &RiskFactorAdjustableModel::get_risk_factor_expected() const noexcept {
    return risk_factor_expected_;
}

void RiskFactorAdjustableModel::adjust_risk_factors(RuntimeContext &context) const {
    // TODO: for ALL risk factors
}

void RiskFactorAdjustableModel::adjust_risk_factors(
    RuntimeContext &context, const std::vector<core::Identifier> &names) const {
    auto coefficients = get_adjustments(context);

    auto &pop = context.population();
    std::for_each(core::execution_policy, pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }

        auto &table = coefficients.row(person.gender);
        for (auto &factor : table) {
            auto current_value = person.get_risk_factor_value(factor.first);
            auto adjustment = factor.second.at(person.age);
            person.risk_factors.at(factor.first) = current_value + adjustment;
        }
    });

    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<RiskFactorAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(coefficients)));
    }
}

SexAgeTable RiskFactorAdjustableModel::get_adjustments(RuntimeContext &context) const {
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

SexAgeTable RiskFactorAdjustableModel::calculate_adjustments(RuntimeContext &context) const {
    const auto &age_range = context.age_range();
    auto max_age = age_range.upper() + 1;
    auto coefficients = std::map<core::Gender, std::map<core::Identifier, std::vector<double>>>{};

    auto simulated_means = calculate_simulated_mean(context);
    auto &baseline_means = risk_factor_expected_.values;
    for (auto &gender : simulated_means) {
        coefficients.emplace(gender.first, std::map<core::Identifier, std::vector<double>>{});
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

    return SexAgeTable{std::move(coefficients)};
}

SexAgeTable RiskFactorAdjustableModel::calculate_simulated_mean(RuntimeContext &context) const {
    const auto &age_range = context.age_range();
    auto max_age = age_range.upper() + 1;
    auto moments = std::map<core::Gender, std::map<core::Identifier, std::vector<FirstMoment>>>{};

    moments.emplace(core::Gender::male, std::map<core::Identifier, std::vector<FirstMoment>>{});
    moments.emplace(core::Gender::female, std::map<core::Identifier, std::vector<FirstMoment>>{});
    for (const auto &entity : context.population()) {
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

    auto means = std::map<core::Gender, std::map<core::Identifier, std::vector<double>>>{};
    for (auto &gender : moments) {
        means.emplace(gender.first, std::map<core::Identifier, std::vector<double>>{});
        for (auto &factor : gender.second) {
            means.at(gender.first).emplace(factor.first, std::vector<double>(max_age, 0.0));
            for (auto age = age_range.lower(); age <= age_range.upper(); age++) {
                means.at(gender.first).at(factor.first).at(age) = factor.second.at(age).mean();
            }
        }
    }

    return SexAgeTable{std::move(means)};
}

} // namespace hgps
