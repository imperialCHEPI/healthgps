#pragma once
#include <map>

#include "interfaces.h"
#include "mapping.h"

namespace hgps {

	struct Coefficient {
		double value{};
		double pvalue{};
		double tvalue{};
		double std_error{};
	};

	struct LinearModel {
		std::unordered_map<std::string, Coefficient> coefficients;
		std::vector<double> fitted_values;
		std::vector<double> residuals;
		double rsquared{};
	};

	struct HierarchicalLevel {
		std::unordered_map<std::string, int> variables;
		core::DoubleArray2D transition;
		core::DoubleArray2D inverse_transition;
		core::DoubleArray2D residual_distribution;
		core::DoubleArray2D correlation;
		std::vector<double> variances;
	};

	class StaticHierarchicalLinearModel : public HierarchicalLinearModel {
	public:
		StaticHierarchicalLinearModel() = delete;
		StaticHierarchicalLinearModel(
			std::unordered_map<std::string, LinearModel>&& models,
			std::map<int, HierarchicalLevel>&& levels);

		virtual HierarchicalModelType type() const noexcept override;

		virtual std::string name() const noexcept override;

		virtual void generate(RuntimeContext& context) override;

	protected:
		std::unordered_map<std::string, LinearModel> models_;
		std::map<int, HierarchicalLevel> levels_;

		virtual void generate_for_entity(RuntimeContext& context, Person& entity, 
			int level, std::vector<MappingEntry>& level_factors);
	};

	class DynamicHierarchicalLinearModel final : public StaticHierarchicalLinearModel {
	public:
		DynamicHierarchicalLinearModel() = delete;

		DynamicHierarchicalLinearModel(
			std::unordered_map<std::string, LinearModel>&& models,
			std::map<int, HierarchicalLevel>&& levels);

		HierarchicalModelType type() const noexcept override;

		std::string name() const noexcept override;

		void generate_for_entity(RuntimeContext& context, Person& entity, 
			int level, std::vector<MappingEntry>& level_factors)  override;
	};
}