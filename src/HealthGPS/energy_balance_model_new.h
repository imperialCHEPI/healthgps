#pragma once

#include "hierarchical_model_types.h"
#include "random_algorithm.h"

namespace hgps {

	/// @brief Implements the dynamic hierarchical linear model (energy balance) type
	/// 
	/// @details The dynamic model is used to advance the virtual population over time.
	class EnergyBalanceHierarchicalModelNew final : public HierarchicalLinearModel {
	public:
		EnergyBalanceHierarchicalModelNew() = delete;

		/// @brief Initialises a new instance of the EnergyBalanceHierarchicalModelNew class
		/// @param definition The model definition instance
		EnergyBalanceHierarchicalModelNew(EnergyBalanceModelDefinition& definition);

		HierarchicalModelType type() const noexcept override;

		const std::string& name() const noexcept override;

		/// @copydoc HierarchicalLinearModel::generate_risk_factors
		/// @throws std::logic_error the dynamic model does not generate risk factors.
		void generate_risk_factors(RuntimeContext& context) override;

		void update_risk_factors(RuntimeContext& context) override;

	private:
		std::reference_wrapper<EnergyBalanceModelDefinition> definition_;
		std::string name_{ "Dynamic" };

		void update_risk_factors_exposure(RuntimeContext& context, Person& entity,
			const std::map<core::Identifier, double>& current_risk_factors,
			const std::map<core::Identifier, FactorDynamicEquation>& equations);

		std::map<core::Identifier, double> get_current_risk_factors(
			const HierarchicalMapping& mapping, Person& entity, int time_year) const;

		double sample_normal_with_boundary(Random& random,
			double mean, double standard_deviation, double boundary) const;
	};
}
