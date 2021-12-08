#pragma once

#include "hierarchical_model_types.h"

namespace hgps {

	class StaticHierarchicalLinearModel final : public HierarchicalLinearModel {
	public:
		StaticHierarchicalLinearModel() = delete;
		StaticHierarchicalLinearModel(HierarchicalLinearModelDefinition& definition);

		HierarchicalModelType type() const noexcept override;

		std::string name() const noexcept override;

		void generate_risk_factors(RuntimeContext& context) override;

		void update_risk_factors(RuntimeContext& context) override;

	private:
		HierarchicalLinearModelDefinition& definition_;

		void generate_for_entity(RuntimeContext& context, Person& entity,
			int level, std::vector<MappingEntry>& level_factors);
	};
}
