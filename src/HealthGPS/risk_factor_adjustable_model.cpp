#include "HealthGPS.Core/exception.h"

#include "risk_factor_adjustable_model.h"
#include "sync_message.h"

#include <oneapi/tbb/parallel_for_each.h>
#include <utility>
#include <iostream>
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
    double expected = expected_->at(sex, factor).at(age);

    // Apply optional trend to expected value.
    if (apply_trend) {
        int elapsed_time = context.time_now() - context.start_time();
        int t = std::min(elapsed_time, get_trend_steps(factor));
        expected *= pow(expected_trend_->at(factor), t);
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
        // More robust retry mechanism for receiving messages
        int retry_count = 0;
        const int max_retries = 5;
        
        while (retry_count < max_retries) {
            // Exponential backoff wait between retries (100ms, 200ms, 400ms, 800ms, 1600ms)
            if (retry_count > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << (retry_count - 1))));
            }
            
            auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
            if (message.has_value()) {
                auto &basePtr = message.value();
                auto *messagePrt = dynamic_cast<RiskFactorAdjustmentMessage *>(basePtr.get());
                if (messagePrt) {
                    // Success
                    adjustments = messagePrt->data();
                    break;  // Exit the retry loop
                }
                
                // Wrong message type
                std::cerr << "Warning: Received wrong adjustment message type, retrying (" 
                          << retry_count + 1 << "/" << max_retries << ")" << std::endl;
            } else {
                // Timeout
                std::cerr << "Warning: Adjustment message receive timed out, retrying (" 
                          << retry_count + 1 << "/" << max_retries << ")" << std::endl;
            }
            
            retry_count++;
        }
        
        // Check if we successfully got the adjustments
        if (retry_count >= max_retries) {
            throw core::HgpsException(
                fmt::format("Simulation out of sync, receive baseline adjustments message has timed out after {} retries.",
                max_retries));
        }
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

int RiskFactorAdjustableModel::get_trend_steps(const core::Identifier &factor) const {
    try {
        if (trend_steps_ && trend_steps_->count(factor) > 0) {
            return trend_steps_->at(factor);
        } else {
            std::cout << "[WARNING-RFAM] No trend steps found for " << factor.to_string() 
                      << ", using default of 5 years" << std::endl;
            return 5; // Default trend steps if not specified
        }
    } catch (const std::exception &e) {
        std::cout << "[ERROR-RFAM] Exception in get_trend_steps for " << factor.to_string() 
                  << ": " << e.what() << ", using default of 5 years" << std::endl;
        return 5; // Default trend steps in case of error
    }
}

RiskFactorSexAgeTable
RiskFactorAdjustableModel::calculate_adjustments(RuntimeContext &context,
                                                 const std::vector<core::Identifier> &factors,
                                                 OptionalRanges ranges, bool apply_trend) const {
    auto age_range = context.age_range();
    auto age_count = age_range.upper() + 1;

    // Compute simulated means.
    auto simulated_means = calculate_simulated_mean(context.population(), age_range, factors);

    // Compute adjustments.
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
