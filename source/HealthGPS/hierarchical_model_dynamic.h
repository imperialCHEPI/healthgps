#pragma once
#include "hierarchical_model_types.h"

namespace hgps {

	class DynamicHierarchicalLinearModel final : public HierarchicalLinearModel {
	public:
		DynamicHierarchicalLinearModel() = delete;
		DynamicHierarchicalLinearModel(
			std::unordered_map<std::string, LinearModel>&& models,
			std::map<int, HierarchicalLevel>&& levels);

		HierarchicalModelType type() const noexcept override;

		std::string name() const noexcept override;

		void generate_risk_factors(RuntimeContext& context) override;

		void update_risk_factors(RuntimeContext& context) override;

	private:
		std::unordered_map<std::string, LinearModel> models_;
		std::map<int, HierarchicalLevel> levels_;

		void generate_for_entity(RuntimeContext& context, Person& entity,
			int level, std::vector<MappingEntry>& level_factors);

		void update_for_entity(RuntimeContext& context, Person& entity,
			const int& level, const std::vector<MappingEntry>& level_factors,
			std::map<std::string,double>& current_risk_factors);

		std::map<std::string, double> get_next_risk_factors(
			const HierarchicalMapping& mapping, Person& entity, int time_year) const;
		std::map<std::string, double> get_current_risk_factors(
			const HierarchicalMapping& mapping, Person& entity, int time_year) const;
	};
}

