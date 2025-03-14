#pragma once
#include "intervention_scenario.h"

#include <functional>
#include <set>
#include <vector>

namespace hgps {

/// @brief Dynamic marketing intervention definition data type
struct MarketingDynamicDefinition {
    MarketingDynamicDefinition() = delete;

    /// @brief Initialise a new instance of the MarketingDynamicDefinition structure
    /// @param period The implementation period
    /// @param sorted_impacts The impacts on risk factors
    /// @param dynamic The population dynamic parameters
    MarketingDynamicDefinition(const PolicyInterval &period,
                               std::vector<PolicyImpact> sorted_impacts, PolicyDynamic dynamic)
        : active_period{period}, impacts{std::move(sorted_impacts)}, dynamic{dynamic} {}

    /// @brief Intervention active period
    PolicyInterval active_period;

    /// @brief Intervention impacts on risk factors
    std::vector<PolicyImpact> impacts;

    /// @brief Population dynamic parameters
    PolicyDynamic dynamic;
};

/// @brief Implements the dynamic marketing regulation intervention scenario
class MarketingDynamicScenario final : public DynamicInterventionScenario {
  public:
    MarketingDynamicScenario() = delete;

    /// @brief Initialises a new instance of the MarketingDynamicScenario class.
    /// @param data_sync The data synchronisation channel instance to use.
    /// @param definition The intervention definition
    /// @throws std::invalid_argument number of impact levels mismatch.
    /// @throws std::out_of_range for overlapping or non-ordered impact levels.
    MarketingDynamicScenario(SyncChannel &data_sync, MarketingDynamicDefinition &&definition);

    SyncChannel &channel() override;

    void clear() noexcept override;

    double apply(Random &generator, Person &entity, int time,
                 const core::Identifier &risk_factor_key, double value) override;

    const PolicyInterval &active_period() const noexcept override;

    const std::vector<PolicyImpact> &impacts() const noexcept override;

    const PolicyDynamic &dynamic() const noexcept override;

    std::string name() override { return "Marketing Dynamic"; }

  private:
    std::reference_wrapper<SyncChannel> channel_;
    MarketingDynamicDefinition definition_;
    std::set<core::Identifier> factor_impact_;
    std::unordered_map<std::size_t, int> interventions_book_{};

    int get_index_of_impact_by_age(unsigned int age) const;
    double get_differential_impact(int current_index, int previous_index) const;
};
} // namespace hgps
