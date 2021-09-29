#include "Intervention_scenario.h"
#include "HealthGPS.Core/string_util.h"

namespace hgps {

	InterventionScenario::InterventionScenario(SyncChannel& data_sync, PolicyDefinition&& definition)
		: channel_{ data_sync }, factor_impact_{}, definition_{std::move(definition)}
	{
		for (auto& factor : definition_.impacts) {
			factor_impact_.emplace(core::to_lower(factor.risk_factor), factor);
		}
	}

	ScenarioType InterventionScenario::type() const noexcept {
		return ScenarioType::intervention;
	}

	std::string InterventionScenario::name() const noexcept {
		return "Intervention";
	}

	SyncChannel& InterventionScenario::channel() {
		return channel_;
	}

	const PolicyImpactType& InterventionScenario::impact_type() const noexcept {
		return definition_.impact_type;
	}

	const PolicyInterval& InterventionScenario::active_period() const noexcept {
		return definition_.active_period;
	}

	const std::vector<PolicyImpact>& InterventionScenario::impacts() const noexcept {
		return definition_.impacts;
	}

	double InterventionScenario::apply(const int& time, const std::string& risk_factor_key, const double& value) {
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
