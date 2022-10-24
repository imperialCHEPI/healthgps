#pragma once
#include "intervention_scenario.h"

#include <set>
#include <functional>

namespace hgps {

	struct MarketingPolicyDefinition
	{
		MarketingPolicyDefinition() = delete;
		MarketingPolicyDefinition(const PolicyInterval& period, const std::vector<PolicyImpact>& sorted_impacts)
			: active_period{ period }, impacts{ sorted_impacts } {}

		PolicyInterval active_period;
		std::vector<PolicyImpact> impacts;
	};

	class MarketingPolicyScenario : public InterventionScenario {
	public:
		MarketingPolicyScenario() = delete;
		MarketingPolicyScenario(SyncChannel& data_sync, MarketingPolicyDefinition&& definition);

		ScenarioType type() const noexcept override;

		std::string name() const noexcept override;

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Random& generator, Person& entity, int time,
			const std::string& risk_factor_key, double value) override;

		const PolicyInterval& active_period()  const noexcept override;

		const std::vector<PolicyImpact>& impacts() const noexcept override;

	private:
		std::reference_wrapper<SyncChannel> channel_;
		MarketingPolicyDefinition definition_;
		std::set<std::string> factor_impact_;
		std::unordered_map<std::size_t, int> interventions_book_{};
	};
}

