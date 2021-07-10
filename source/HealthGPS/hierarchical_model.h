#pragma once
#include <map>

#include "interfaces.h"
#include "runtime_context.h"

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
		std::vector<std::string> variables;
		core::DoubleArray2D transition;
		core::DoubleArray2D inverse_transition;
		core::DoubleArray2D residual_distribution;
		core::DoubleArray2D correlation;
		std::vector<double> variances;
	};

	class HierarchicalLinearModel {
	public:
		HierarchicalLinearModel(
			std::vector<std::string>& exclusions,
			std::unordered_map<std::string, LinearModel>&& models,
			std::map<int, HierarchicalLevel>&& levels);

		virtual ~HierarchicalLinearModel() = default;

		virtual HierarchicalModelType type() const;

		virtual std::string name() const;

		virtual void generate(RuntimeContext& context);

	protected:
		HierarchicalLinearModel() = default;
		std::vector<std::string> exclusions_;
		std::unordered_map<std::string, LinearModel> models_;
		std::map<int, HierarchicalLevel> levels_;
	};

	class DynamicHierarchicalLinearModel final : public HierarchicalLinearModel {
	public:
		DynamicHierarchicalLinearModel() = delete;

		DynamicHierarchicalLinearModel(
			std::vector<std::string>& exclusions,
			std::unordered_map<std::string, LinearModel>&& models,
			std::map<int, HierarchicalLevel>&& levels);

		HierarchicalModelType type() const override;

		std::string name() const override;

		void generate(RuntimeContext& context)  override;
	};
}