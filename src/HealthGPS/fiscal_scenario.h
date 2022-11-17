#pragma once
#include "intervention_scenario.h"

#include <cstdint>
#include <string>
#include <set>
#include <functional>

namespace hgps {

	/// @brief Health GPS policy impact types enumeration
	enum class FiscalImpactType : uint8_t
	{
		/// @brief variable impact overtime    - option 1
		pessimist,

		/// @brief incremental impact overtime - option 2
		optimist
	};

	struct FiscalPolicyDefinition
	{
		FiscalPolicyDefinition() = delete;
		FiscalPolicyDefinition(const FiscalImpactType type_of_impact,
			const PolicyInterval& period, const std::vector<PolicyImpact>& sorted_impacts)
			: impact_type{ type_of_impact }, active_period{ period }, impacts{ sorted_impacts } {}

		FiscalImpactType impact_type;
		PolicyInterval active_period;
		std::vector<PolicyImpact> impacts;
	};

	class FiscalPolicyScenario final : public InterventionScenario {
	public:
		FiscalPolicyScenario() = delete;
		FiscalPolicyScenario(SyncChannel& data_sync, FiscalPolicyDefinition&& definition);

		ScenarioType type() const noexcept override;

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Random& generator, Person& entity, int time,
			const core::Identifier& risk_factor_key, double value) override;

		const PolicyInterval& active_period()  const noexcept override;

		const std::vector<PolicyImpact>& impacts() const noexcept override;

	private:
		std::reference_wrapper<SyncChannel> channel_;
		FiscalPolicyDefinition definition_;
		std::set<core::Identifier> factor_impact_;
		std::unordered_map<std::size_t, int> interventions_book_{};
	};

	FiscalImpactType parse_fiscal_impact_type(std::string impact);
}
