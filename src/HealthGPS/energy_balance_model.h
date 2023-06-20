#pragma once

#include "hierarchical_model_types.h"
#include "random_algorithm.h"

namespace hgps {

	/// @brief Implements the energy balance model type
	/// 
	/// @details The dynamic model is used to advance the virtual population over time.
	class EnergyBalanceModel final : public HierarchicalLinearModel {
	public:
		EnergyBalanceModel() = delete;

		/// @brief Initialises a new instance of the EnergyBalanceModel class
		/// @param definition The model definition instance
		EnergyBalanceModel(EnergyBalanceModelDefinition& definition);

		HierarchicalModelType type() const noexcept override;

		const std::string& name() const noexcept override;

		/// @copydoc HierarchicalLinearModel::generate_risk_factors
		/// @throws std::logic_error the dynamic model does not generate risk factors.
		void generate_risk_factors(RuntimeContext& context) override;

		void update_risk_factors(RuntimeContext& context) override;

	private:
		std::reference_wrapper<EnergyBalanceModelDefinition> definition_;
		std::string name_{ "Dynamic" };

		std::map<core::Identifier, double> get_current_risk_factors(
			const HierarchicalMapping& mapping, Person& entity, int time_year) const;
	};
}
