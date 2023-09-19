#pragma once
#include "scenario.h"

#include <optional>
#include <vector>

namespace hgps {

struct PolicyImpact;
struct PolicyInterval;
struct PolicyDynamic;

/// @brief Health-GPS scripted intervention policy scenario interface
///
/// @details The intervention scenario interface must be implemented by
/// all intervention policies, the interface identify the type as
/// ScenarioType::intervention and name, and extends the Scenario with
/// common intervention properties: active period and impacts.
class InterventionScenario : public Scenario {
  public:
    /// @brief Gets the intervention active period
    /// @return Intervention period
    virtual const PolicyInterval &active_period() const noexcept = 0;

    /// @brief Gets the intervention impacts by risk factor and age range
    /// @return Intervention impact on risk factors
    virtual const std::vector<PolicyImpact> &impacts() const noexcept = 0;

    ScenarioType type() const noexcept override { return ScenarioType::intervention; }

    const std::string &name() const noexcept override { return name_; }

  private:
    std::string name_{"Intervention"};
};

/// @brief Health-GPS dynamic intervention policy scenario interface
///
/// @details The dynamic intervention scenario adds support for System Dynamic
/// equations to smooth the population effected by the intervention over-time.
class DynamicInterventionScenario : public InterventionScenario {
  public:
    /// @brief Gets the System Dynamic model parameters
    /// @return Intervention dynamic parameters
    virtual const PolicyDynamic &dynamic() const noexcept = 0;
};

/// @brief Defines the policy impact on risk factors data structure
struct PolicyImpact {
    PolicyImpact() = delete;

    /// @brief Initialise a new instance of the PolicyImpact structure.
    /// @param risk_factor_key Risk factor identifier
    /// @param policy_impact The impact on risk factor value
    /// @param start_age Impact start age, inclusive
    /// @param end_age Impact end age, inclusive
    /// @throws std::out_of_range for impact end age less than the start age.
    PolicyImpact(core::Identifier risk_factor_key, double policy_impact, unsigned int start_age,
                 std::optional<unsigned int> end_age = std::nullopt)
        : risk_factor{std::move(risk_factor_key)}, value{policy_impact}, from_age{start_age},
          to_age{end_age} {

        if (end_age.has_value() && start_age > end_age.value()) {
            throw std::out_of_range("Impact end age must be equal or greater than the start age.");
        }
    }

    /// @brief The risk factor identifier
    core::Identifier risk_factor{};

    /// @brief The effect value
    double value{};

    /// @brief Impact applies from age
    unsigned int from_age{};

    /// @brief Impact applies to age, if no value, no upper limit
    std::optional<unsigned int> to_age{};

    // NOLINTBEGIN(bugprone-exception-escape)
    /// @brief Determine whether this impact should be applied to a given age
    /// @param age The age to check
    /// @return true, if the age is in the impact valid range; otherwise, false.
    bool contains(unsigned int age) const noexcept {
        if (age < from_age) {
            return false;
        }

        if (to_age.has_value()) {
            return age <= to_age.value();
        }

        return true;
    }
    // NOLINTEND(bugprone-exception-escape)
};

/// @brief Defines the policy active interval
struct PolicyInterval {

    /// @brief Initialise a new instance of the PolicyInterval structure.
    /// @param start_at_time Intervention start time, inclusive
    /// @param finish_at_time Intervention finish time, or no upper bound
    /// @throws std::out_of_range for policy with negative start time or finishing before start time
    PolicyInterval(int start_at_time, std::optional<int> finish_at_time = std::nullopt)
        : start_time{start_at_time}, finish_time{finish_at_time} {
        if (start_at_time < 0) {
            throw std::out_of_range("Policy start time must not be negative.");
        }

        if (finish_at_time.has_value() && start_at_time > finish_at_time.value()) {
            throw std::out_of_range(
                "Policy finish time must be equal or greater than the start time.");
        }
    }

    /// @brief The intervention start time
    int start_time{};
    /// @brief The intervention finish time, if given, or no upper bound.
    std::optional<int> finish_time{};

    // NOLINTBEGIN(bugprone-exception-escape)
    /// @brief Determine whether this interval contains a given time
    /// @param time The time to check
    /// @return true, if the time is in the interval range; otherwise, false.
    bool contains(int time) const noexcept {
        if (time < start_time) {
            return false;
        }

        if (finish_time.has_value()) {
            return time <= finish_time.value();
        }

        return true;
    }
    // NOLINTEND(bugprone-exception-escape)
};

/// @brief Defines the policy dynamic parameters
struct PolicyDynamic {
    /// @brief Initialises a new instance of the PolicyDynamic structure
    /// @param parameters The System Dynamic parameters
    /// @throws std::invalid_argument for parameters size different of three [alpha, beta, gamma]
    /// @throws std::out_of_range for parameters value outside of the valid range [0.0, 1.0]
    PolicyDynamic(const std::vector<double> &parameters) {
        if (parameters.size() != size_t{3}) {
            throw std::invalid_argument(
                "The vector of dynamic arguments must have size = 3 [alpha, beta, gamma]");
        }

        for (const auto &p : parameters) {
            if (0.0 > p || p > 1.0) {
                throw std::out_of_range("Dynamic argument: " + std::to_string(p) +
                                        ", outside range [0.0, 1.0]");
            }
        }

        alpha = parameters.at(0);
        beta = parameters.at(1);
        gamma = parameters.at(2);
    }

    /// @brief Alpha parameter, default 1.0 (full effect at start)
    double alpha{1.0};

    /// @brief Beta parameter, default 0.0 (no effect)
    double beta{0.0};

    /// @brief Gamma parameter, default 0.0 (no effect)
    double gamma{0.0};
};
} // namespace hgps
