#pragma once

#include "hierarchical_model_types.h"
#include <functional>

namespace hgps {

	/// @brief Implements the static hierarchical linear model type
	///
	/// @details The static model is used to initialise the virtual population,
	/// the model uses principal component analysis for residual normalisation.
	class StaticHierarchicalLinearModel final : public HierarchicalLinearModel {
	public:
		StaticHierarchicalLinearModel() = delete;

		/// @brief Initialises a new instance of the StaticHierarchicalLinearModel class
		/// @param definition The model definition instance
		StaticHierarchicalLinearModel(HierarchicalLinearModelDefinition& definition);

		HierarchicalModelType type() const noexcept override;

		const std::string& name() const noexcept override;

		void generate_risk_factors(RuntimeContext& context) override;

		void update_risk_factors(RuntimeContext& context) override;

	private:
		std::reference_wrapper<HierarchicalLinearModelDefinition> definition_;
		std::string name_{ "Static" };

		void generate_for_entity(RuntimeContext& context, Person& entity,
			int level, std::vector<MappingEntry>& level_factors);
	};
}
