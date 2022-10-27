#pragma once
#include "intervention_scenario.h"

#include <set>
#include <vector>
#include <functional>

namespace hgps {
	struct MarketingDynamicDefinition
	{
		MarketingDynamicDefinition() = delete;
		MarketingDynamicDefinition(const PolicyInterval& period, const std::vector<PolicyImpact>& sorted_impacts, PolicyDynamic dynamic)
			: active_period{ period }, impacts{ sorted_impacts }, dynamic{ std::move(dynamic) } {}

		PolicyInterval active_period;
		std::vector<PolicyImpact> impacts;
		PolicyDynamic dynamic;
	};

	class MarketingDynamicScenario : public DynamicInterventionScenario
	{
	public:
		MarketingDynamicScenario() = delete;
		MarketingDynamicScenario(SyncChannel& data_sync, MarketingDynamicDefinition&& definition);

		ScenarioType type() const noexcept override;

		std::string name() const noexcept override;

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Random& generator, Person& entity, int time,
			const core::Identifier& risk_factor_key, double value) override;

		const PolicyInterval& active_period()  const noexcept override;

		const std::vector<PolicyImpact>& impacts() const noexcept override;

		const PolicyDynamic& dynamic() const noexcept override;

	private:
		std::reference_wrapper<SyncChannel> channel_;
		MarketingDynamicDefinition definition_;
		std::set<core::Identifier> factor_impact_;
		std::unordered_map<std::size_t, int> interventions_book_{};

		int get_index_of_impact_by_age(const unsigned int& age) const noexcept;
		double get_differential_impact(int current_index, int previous_index) const;
	};
}