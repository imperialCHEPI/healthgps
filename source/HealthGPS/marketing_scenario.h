#pragma once
#include "intervention_scenario.h"
#include <set>

namespace hgps {

	struct MarketingPolicyDefinition
	{
		MarketingPolicyDefinition() = delete;
		MarketingPolicyDefinition(const PolicyInterval& period, const std::vector<PolicyImpact>& sorted_impacts)
			: active_period{ period }, impacts{ sorted_impacts } {}

		const std::vector<PolicyImpact> impacts;
		const PolicyInterval active_period;
	};

	class MarketingPolicyScenario : public InterventionScenario {
	public:
		MarketingPolicyScenario() = delete;
		MarketingPolicyScenario(SyncChannel& data_sync, MarketingPolicyDefinition&& definition);

		ScenarioType type() const noexcept override;

		std::string name() const noexcept override;

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Person& entity, const int& time,
			const std::string& risk_factor_key, const double& value) override;

		const PolicyInterval& active_period()  const noexcept override;

		const std::vector<PolicyImpact>& impacts() const noexcept override;

	private:
		SyncChannel& channel_;
		MarketingPolicyDefinition definition_;
		std::set<std::string> factor_impact_;
		std::unordered_map<std::size_t, int> interventions_book_{};
	};
}

