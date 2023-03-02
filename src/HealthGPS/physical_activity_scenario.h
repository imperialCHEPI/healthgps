#pragma once
#include "intervention_scenario.h"

#include <set>
#include <unordered_map>
#include <functional>

namespace hgps {

	/// @brief Physical activity intervention definition data type
	struct PhysicalActivityDefinition
	{
		/// @brief Intervention active period
		PolicyInterval active_period;

		/// @brief Intervention impacts on risk factors
		std::vector<PolicyImpact> impacts;
		
		/// @brief Intervention coverage rate
		double coverage_rate{};
	};

	/// @brief Implements the physical activity regulation intervention scenario
	class PhysicalActivityScenario final : public InterventionScenario
	{
	public:
		PhysicalActivityScenario() = delete;

		/// @brief Initialises a new instance of the MarketingDynamicScenario class.
		/// @param data_sync The data synchronisation channel instance to use.
		/// @param definition The intervention definition
		/// @throws std::invalid_argument number of impact levels mismatch.
		/// @throws std::out_of_range for overlapping or non-ordered impact levels.
		PhysicalActivityScenario(SyncChannel& data_sync, PhysicalActivityDefinition&& definition);

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Random& generator, Person& entity, int time,
			const core::Identifier& risk_factor_key, double value) override;

		const PolicyInterval& active_period()  const noexcept override;

		const std::vector<PolicyImpact>& impacts() const noexcept override;
	private:
		std::reference_wrapper<SyncChannel> channel_;
		PhysicalActivityDefinition definition_;
		std::set<core::Identifier> factor_impact_;
		std::unordered_map<std::size_t, int> interventions_book_{};
	};
}
