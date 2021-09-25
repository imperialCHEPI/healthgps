#pragma once
#include "interfaces.h"
#include "mapping.h"
#include "map2d.h"
#include "gender_value.h"

namespace hgps {

	struct HierarchicalLinearModelDefinition;
	using BaselineAdjustmentTable = Map2d<int, std::string, DoubleGenderValue>;
	using HLMDefinitionMap = std::map<HierarchicalModelType, HierarchicalLinearModelDefinition>;

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

	struct BaselineAdjustment {
		BaselineAdjustment() = default;
		BaselineAdjustment(BaselineAdjustmentTable&& baseline_averages)
			: averages{ baseline_averages }, risk_factors{}, is_enabled{ true }
		{
			if (baseline_averages.empty()) {
				throw std::invalid_argument(
					"The baseline risk factor averages table must not be empty.");
			}

			for (const auto& factor : averages.cbegin()->second) {
				risk_factors.emplace_back(factor.first);
			}
		}

		const bool is_enabled{ false };
		const BaselineAdjustmentTable averages{};
		std::vector<std::string> risk_factors{};
	};

	struct HierarchicalLinearModelDefinition
	{
		HierarchicalLinearModelDefinition() = delete;
		HierarchicalLinearModelDefinition(
			std::unordered_map<std::string, LinearModel>&& linear_models,
			std::map<int, HierarchicalLevel>&& model_levels,
			const BaselineAdjustment& baseline_adjustment)
			: models{ std::move(linear_models) }, levels{ std::move(model_levels) },
			adjustments{ baseline_adjustment } {}

		const std::unordered_map<std::string, LinearModel> models;
		const std::map<int, HierarchicalLevel> levels;
		const BaselineAdjustment& adjustments;
	};
}