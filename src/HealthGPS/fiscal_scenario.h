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

	/// @brief Fiscal policy intervention definition data type
	struct FiscalPolicyDefinition
	{
		FiscalPolicyDefinition() = delete;

		/// @brief Initialise a new instance of the FiscalPolicyDefinition structure
		/// @param type_of_impact The fiscal impact type
		/// @param period The intervention implementation period
		/// @param sorted_impacts The impacts on risk factors
		FiscalPolicyDefinition(const FiscalImpactType type_of_impact,
			const PolicyInterval& period, const std::vector<PolicyImpact>& sorted_impacts)
			: impact_type{ type_of_impact }, active_period{ period }, impacts{ sorted_impacts } {}

		/// @brief The fiscal impact type
		FiscalImpactType impact_type;

		/// @brief Intervention active period
		PolicyInterval active_period;

		/// @brief Intervention impacts on risk factors
		std::vector<PolicyImpact> impacts;
	};

	/// @brief Implements the fiscal policy intervention scenario
	class FiscalPolicyScenario final : public InterventionScenario {
	public:
		FiscalPolicyScenario() = delete;

		/// @brief Initialises a new instance of the MarketingDynamicScenario class
		/// @param data_sync The data synchronisation channel instance to use
		/// @param definition The intervention definition
		/// @throws std::invalid_argument for number of impact levels mismatch.
		/// @throws std::out_of_range for overlapping or non-ordered impact levels.
		FiscalPolicyScenario(SyncChannel& data_sync, FiscalPolicyDefinition&& definition);

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
