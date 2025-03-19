#pragma once
#include "gender_value.h"
#include "intervention_scenario.h"

#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace hgps {

/// @brief Defined the risk factor adjustment data type
struct AdjustmentFactor {
    /// @brief Initialise a new instance of the AdjustmentFactor structure
    /// @param factor_name The risk factor name
    /// @param value The adjustment value
    AdjustmentFactor(std::string factor_name, double value)
        : identifier{factor_name}, adjustment{value} {}

    /// @brief The risk factor identifier
    core::Identifier identifier;

    /// @brief The adjustment value
    double adjustment{};
};

/// @brief Define the policy population coverage data type
struct PolicyCoverage {
    PolicyCoverage() = delete;

    /// @brief Initialise a new instance of the PolicyCoverage structure
    /// @param rates Transfer coefficient rates
    /// @param effect_time The effect cut-off time
    /// @throws std::invalid_argument for transfer coefficient rates size mismatch
    PolicyCoverage(const std::vector<double> &rates, unsigned int effect_time)
        : cutoff_time{effect_time} {
        if (rates.size() != 2) {
            throw std::invalid_argument(
                "The number of transfer coefficients must be 2 (short, long) term.");
        }

        short_term_rate = rates[0];
        long_term_rate = rates[1];
    }

    /// @brief The short term transfer coefficient value
    double short_term_rate{};

    /// @brief The long term transfer coefficient value
    double long_term_rate{};

    /// @brief The coverage effect cut-off time
    unsigned int cutoff_time{};
};

/// @brief Define the transfer coefficient data type
struct TransferCoefficient {
    TransferCoefficient() = delete;

    /// @brief Initialise a new instance of the TransferCoefficient structure
    /// @param values The transfer coefficient values
    /// @param child_age The child cut-off age (before adult)
    /// @throws std::out_of_range for number of transfer coefficients mismatch.
    TransferCoefficient(std::vector<double> values, unsigned int child_age)
        : child{}, adult{}, child_cutoff_age{child_age} {
        if (values.size() != 4) {
            throw std::out_of_range("The number of transfer coefficients must be 4.");
        }

        child.males = values[0];
        child.females = values[1];
        adult.males = values[2];
        adult.females = values[3];
    }

    /// @brief Children transfer coefficient values
    DoubleGenderValue child;

    /// @brief Adult transfer coefficient values
    DoubleGenderValue adult;

    /// @brief The child cut-off age (before adult)
    unsigned int child_cutoff_age{};

    /// @brief Gets the transfer coefficient value for a given gender and age
    /// @param gender The gender reference
    /// @param age The age reference
    /// @return The transfer coefficient value
    double get_value(core::Gender gender, unsigned int age) const noexcept {
        if (gender == core::Gender::male) {
            if (age <= child_cutoff_age) {
                return child.males;
            }

            return adult.males;
        }

        if (age <= child_cutoff_age) {
            return child.females;
        }

        return adult.females;
    }
};

/// @brief Food labelling intervention definition data type
struct FoodLabellingDefinition {
    /// @brief Intervention active period
    PolicyInterval active_period;

    /// @brief Intervention impacts on risk factors
    std::vector<PolicyImpact> impacts;

    /// @brief The adjustments on risk factors
    AdjustmentFactor adjustment_risk_factor;

    /// @brief The intervention population coverage
    PolicyCoverage coverage;

    /// @brief The transfer coefficient values
    TransferCoefficient transfer_coefficient;
};

/// @brief Implements the food labelling intervention scenario
class FoodLabellingScenario final : public InterventionScenario {
  public:
    FoodLabellingScenario() = delete;

    /// @brief Initialises a new instance of the FoodLabellingScenario class
    /// @param data_sync The data synchronisation channel instance to use
    /// @param definition The intervention definition
    /// @throws std::invalid_argument number of impact levels mismatch.
    /// @throws std::out_of_range for overlapping or non-ordered impact levels.
    FoodLabellingScenario(SyncChannel &data_sync, FoodLabellingDefinition &&definition);

    SyncChannel &channel() override;

    void clear() noexcept override;

    double apply(Random &generator, Person &entity, int time,
                 const core::Identifier &risk_factor_key, double value) override;

    const PolicyInterval &active_period() const noexcept override;

    const std::vector<PolicyImpact> &impacts() const noexcept override;

    std::string name() override { return "Food Labelling"; }

  private:
    std::reference_wrapper<SyncChannel> channel_;
    FoodLabellingDefinition definition_;
    std::set<core::Identifier> factor_impact_;
    std::unordered_map<std::size_t, int> interventions_book_{};

    double calculate_policy_impact(const Person &entity) const noexcept;
};
} // namespace hgps
