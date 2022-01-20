#pragma once

#include "hierarchical_model_types.h"
#include "random_algorithm.h"

namespace hgps {

	class EnergyBalanceHierarchicalModel final : public HierarchicalLinearModel {
	public:
		EnergyBalanceHierarchicalModel() = delete;
		EnergyBalanceHierarchicalModel(LiteHierarchicalModelDefinition& definition);

		HierarchicalModelType type() const noexcept override;

		std::string name() const noexcept override;

		void generate_risk_factors(RuntimeContext& context) override;

		void update_risk_factors(RuntimeContext& context) override;

	private:
		LiteHierarchicalModelDefinition& definition_;

		void update_risk_factors_exposure(RuntimeContext& context, Person& entity,
			std::map<std::string, double>& current_risk_factors,
			std::map<std::string, FactorDynamicEquation>& equations);

		std::map<std::string, double> get_current_risk_factors(
			const HierarchicalMapping& mapping, Person& entity, int time_year) const;

		double sample_normal_with_boundary(Random& random, const double mean,
			const double standard_deviation, const double boundary) const;
	};
}