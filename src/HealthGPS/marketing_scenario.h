#pragma once
#include "intervention_scenario.h"

#include <functional>
#include <set>

namespace hgps {

/// @brief Marketing intervention definition data type
struct MarketingPolicyDefinition {
    MarketingPolicyDefinition() = delete;
    /// @brief Initialise a new instance of the MarketingPolicyDefinition structure
    /// @param period The implementation period
    /// @param sorted_impacts The impacts on risk factors
    MarketingPolicyDefinition(const PolicyInterval &period,
                              std::vector<PolicyImpact> sorted_impacts)
        : active_period{period}, impacts{std::move(sorted_impacts)} {}

    /// @brief Intervention active period
    PolicyInterval active_period;

    /// @brief Intervention impacts on risk factors
    std::vector<PolicyImpact> impacts;
};

/// @brief Implements the marketing regulation intervention scenario
///
/// @details This is the initial implementation of the marketing
/// regulation before the dynamic component has been added.
class MarketingPolicyScenario final : public InterventionScenario {
  public:
    MarketingPolicyScenario() = delete;

    /// @brief Initialises a new instance of the MarketingDynamicScenario class.
    /// @param data_sync The data synchronisation channel instance to use.
    /// @param definition The intervention definition
    /// @throws std::invalid_argument number of impact levels mismatch.
    /// @throws std::out_of_range for overlapping or non-ordered impact levels.
    MarketingPolicyScenario(SyncChannel &data_sync, MarketingPolicyDefinition &&definition);

    SyncChannel &channel() override;

    void clear() noexcept override;

    double apply(Random &generator, Person &entity, int time,
                 const core::Identifier &risk_factor_key, double value) override;

    const PolicyInterval &active_period() const noexcept override;

    const std::vector<PolicyImpact> &impacts() const noexcept override;

  private:
    std::reference_wrapper<SyncChannel> channel_;
    MarketingPolicyDefinition definition_;
    std::set<core::Identifier> factor_impact_;
    std::unordered_map<std::size_t, int> interventions_book_{};
};
} // namespace hgps
