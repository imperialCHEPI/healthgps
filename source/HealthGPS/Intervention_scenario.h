#pragma once

#include "policy_scenario.h"

#include <map>
#include <optional>

namespace hgps {

	struct PolicyDefinition {
		PolicyDefinition() = delete;
		PolicyDefinition(const PolicyImpactType& type_of_impact, 
			const std::vector<PolicyImpact>& risk_impacts, const PolicyInterval& period)
			: impact_type{ type_of_impact }, impacts{ risk_impacts }, active_period{ period }{}

		const PolicyImpactType impact_type;
		const std::vector<PolicyImpact> impacts;
		const PolicyInterval active_period;
	};

	class InterventionScenario final : public PolicyScenario {
	public:
		InterventionScenario() = delete;
		InterventionScenario(SyncChannel& data_sync, PolicyDefinition&& definition);

		ScenarioType type() const noexcept override;

		std::string name() const noexcept override;

		SyncChannel& channel() override;

		double apply(const int& time, const std::string& risk_factor_key, const double& value) override;

		const PolicyImpactType& impact_type() const noexcept override;

		const PolicyInterval& active_period()  const noexcept override;

		const std::vector<PolicyImpact>& impacts() const noexcept override;

	private:
		SyncChannel& channel_;
		PolicyDefinition definition_;
		std::map<std::string, PolicyImpact> factor_impact_;
	};
}
