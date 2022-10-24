#include "simple_policy_scenario.h"
#include "HealthGPS.Core/string_util.h"

namespace hgps {

	SimplePolicyScenario::SimplePolicyScenario(SyncChannel& data_sync, SimplePolicyDefinition&& definition)
		: channel_{ data_sync }, definition_{std::move(definition)}, factor_impact_{}
	{
		for (auto& factor : definition_.impacts) {
			factor_impact_.emplace(core::to_lower(factor.risk_factor), factor);
		}
	}

	ScenarioType SimplePolicyScenario::type() const noexcept {
		return ScenarioType::intervention;
	}

	std::string SimplePolicyScenario::name() const noexcept {
		return "Intervention";
	}

	SyncChannel& SimplePolicyScenario::channel() {
		return channel_;
	}

	void SimplePolicyScenario::clear() noexcept {
	}

	const PolicyImpactType& SimplePolicyScenario::impact_type() const noexcept {
		return definition_.impact_type;
	}

	const PolicyInterval& SimplePolicyScenario::active_period() const noexcept {
		return definition_.active_period;
	}

	const std::vector<PolicyImpact>& SimplePolicyScenario::impacts() const noexcept {
		return definition_.impacts;
	}

	double SimplePolicyScenario::apply([[maybe_unused]] Random& generator, [[maybe_unused]] Person& entity,
		int time, const std::string& risk_factor_key, double value) {
		auto result = value;
		if (definition_.active_period.contains(time) && factor_impact_.contains(risk_factor_key)) {
			auto impact = factor_impact_.at(risk_factor_key).value;
			if (definition_.impact_type == PolicyImpactType::absolute) {
				result = impact + value;
			}
			else if (definition_.impact_type == PolicyImpactType::relative) {
				result = (1.0 + impact) * value;
			}
			else {
				throw std::logic_error("Policy impact type not yet implemented.");
			}
		}

		return result;
	}
}
