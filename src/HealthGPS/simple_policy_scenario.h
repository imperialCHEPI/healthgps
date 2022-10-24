#pragma once

#include "intervention_scenario.h"

#include <map>
#include <optional>
#include <functional>

namespace hgps {

	/// @brief Health GPS policy impact types enumeration
	enum class PolicyImpactType : uint8_t
	{
		/// @brief Absolute impact
		absolute,

		/// @brief Relative impact
		relative,
	};

	struct SimplePolicyDefinition {
		SimplePolicyDefinition() = delete;
		SimplePolicyDefinition(const PolicyImpactType& type_of_impact,
			const std::vector<PolicyImpact>& sorted_impacts, const PolicyInterval& period)
			: impact_type{ type_of_impact }, impacts{ sorted_impacts }, active_period{ period }{}

		PolicyImpactType impact_type;
		std::vector<PolicyImpact> impacts;
		PolicyInterval active_period;
	};

	class SimplePolicyScenario final : public InterventionScenario {
	public:
		SimplePolicyScenario() = delete;
		SimplePolicyScenario(SyncChannel& data_sync, SimplePolicyDefinition&& definition);

		ScenarioType type() const noexcept override;

		std::string name() const noexcept override;

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Random& generator, Person& entity, int time,
			const std::string& risk_factor_key, double value) override;

		const PolicyImpactType& impact_type() const noexcept;

		const PolicyInterval& active_period()  const noexcept override;

		const std::vector<PolicyImpact>& impacts() const noexcept override;

	private:
		std::reference_wrapper<SyncChannel> channel_;
		SimplePolicyDefinition definition_;
		std::map<std::string, PolicyImpact> factor_impact_;
	};
}
