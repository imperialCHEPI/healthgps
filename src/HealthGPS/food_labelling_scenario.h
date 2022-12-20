#pragma once
#include "intervention_scenario.h"
#include "gender_value.h"

#include <set>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace hgps {

	struct AdjustmentFactor
	{
		AdjustmentFactor(std::string factor_name, double value)
			: identifier{factor_name}, adjustment{value} {}

		core::Identifier identifier;
		double adjustment{};
	};

	struct PolicyCoverage
	{
		PolicyCoverage() = delete;
		PolicyCoverage(const std::vector<double>& rates, unsigned int effect_time)
			: cutoff_time{ effect_time }
		{
			if (rates.size() != 2) {
				throw std::invalid_argument("The number of transfer coefficients must be 2 (short, long) term.");
			}

			short_term_rate = rates[0];
			long_term_rate = rates[1];
		}

		double short_term_rate{};
		double long_term_rate{};
		unsigned int cutoff_time{};
	};

	struct TransferCoefficient
	{
		TransferCoefficient() = delete;
		TransferCoefficient(std::vector<double> values, unsigned int child_age)
			: child{}, adult{}, child_cutoff_age{ child_age }
		{
			if (values.size() != 4) {
				throw std::invalid_argument("The number of transfer coefficients must be 4.");
			}

			child.males = values[0];
			child.females = values[1];
			adult.males = values[2];
			adult.females = values[3];
		}

		DoubleGenderValue child;
		DoubleGenderValue adult;
		unsigned int child_cutoff_age{};

		double get_value(core::Gender gender, unsigned int age) const noexcept {
			if (gender == core::Gender::male) {
				if (age <= child_cutoff_age) {
					return child.males;
				}

				return adult.males;
			}

			if (age <= child_cutoff_age) {
				return child.females;
			}

			return adult.females;
		}
	};

	struct FoodLabellingDefinition
	{
		PolicyInterval active_period;
		std::vector<PolicyImpact> impacts;
		AdjustmentFactor adjustment_risk_factor;
		PolicyCoverage coverage;
		TransferCoefficient transfer_coefficient;
	};

	class FoodLabellingScenario final : public InterventionScenario
	{
	public:
		FoodLabellingScenario() = delete;
		FoodLabellingScenario(SyncChannel& data_sync, FoodLabellingDefinition&& definition);

		ScenarioType type() const noexcept override;

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Random& generator, Person& entity, int time,
			const core::Identifier& risk_factor_key, double value) override;

		const PolicyInterval& active_period()  const noexcept override;

		const std::vector<PolicyImpact>& impacts() const noexcept override;

	private:
		std::reference_wrapper<SyncChannel> channel_;
		FoodLabellingDefinition definition_;
		std::set<core::Identifier> factor_impact_;
		std::unordered_map<std::size_t, int> interventions_book_{};

		double calculate_policy_impact(const Person& entity) const noexcept;
	};
}