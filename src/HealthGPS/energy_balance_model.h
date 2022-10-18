#pragma once

#include "hierarchical_model_types.h"
#include "random_algorithm.h"

namespace hgps {

	class EnergyBalanceHierarchicalModel final : public HierarchicalLinearModel {
	public:
		EnergyBalanceHierarchicalModel() = delete;
		EnergyBalanceHierarchicalModel(LiteHierarchicalModelDefinition& definition);

		HierarchicalModelType type() const noexcept override;

		const std::string& name() const noexcept override;

		void generate_risk_factors(RuntimeContext& context) override;

		void update_risk_factors(RuntimeContext& context) override;

	private:
		std::reference_wrapper<LiteHierarchicalModelDefinition> definition_;
		std::string name_{ "Dynamic" };

		void update_risk_factors_exposure(RuntimeContext& context, Person& entity,
			const std::map<std::string, double>& current_risk_factors,
			const std::map<std::string, FactorDynamicEquation>& equations);

		std::map<std::string, double> get_current_risk_factors(
			const HierarchicalMapping& mapping, Person& entity, int time_year) const;

		double sample_normal_with_boundary(Random& random,
			double mean, double standard_deviation, double boundary) const;
	};
}