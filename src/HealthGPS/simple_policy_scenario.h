#pragma once

#include "intervention_scenario.h"

#include <functional>
#include <map>
#include <optional>

namespace hgps {

/// @brief Health GPS policy impact types enumeration
enum class PolicyImpactType : uint8_t {
    /// @brief Absolute impact
    absolute,

    /// @brief Relative impact
    relative,
};

/// @brief Simple intervention policy definition data type
struct SimplePolicyDefinition {
    SimplePolicyDefinition() = delete;
    /// @brief Initialise a new instance of the SimplePolicyDefinition structure
    /// @param type_of_impact The policy impact type
    /// @param sorted_impacts The impact on risk factors
    /// @param period The implementation period
    SimplePolicyDefinition(const PolicyImpactType &type_of_impact,
                           std::vector<PolicyImpact> sorted_impacts, const PolicyInterval &period)
        : impact_type{type_of_impact}, impacts{std::move(sorted_impacts)}, active_period{period} {}

    /// @brief Intervention impact type
    PolicyImpactType impact_type;

    /// @brief Intervention impacts on risk factors
    std::vector<PolicyImpact> impacts;

    /// @brief Active implementation period
    PolicyInterval active_period;
};

/// @brief Defines the simple development intervention scenario
class SimplePolicyScenario final : public InterventionScenario {
  public:
    SimplePolicyScenario() = delete;

    /// @brief Initialises a new instance of the SimplePolicyScenario class.
    /// @param data_sync The data synchronisation channel instance to use.
    /// @param definition The intervention definition
    SimplePolicyScenario(SyncChannel &data_sync, SimplePolicyDefinition &&definition);

    SyncChannel &channel() override;

    void clear() noexcept override;

    double apply(Random &generator, Person &entity, int time,
                 const core::Identifier &risk_factor_key, double value) override;

    const PolicyImpactType &impact_type() const noexcept;

    const PolicyInterval &active_period() const noexcept override;

    const std::vector<PolicyImpact> &impacts() const noexcept override;

  private:
    std::reference_wrapper<SyncChannel> channel_;
    SimplePolicyDefinition definition_;
    std::map<core::Identifier, PolicyImpact> factor_impact_;
};
} // namespace hgps
