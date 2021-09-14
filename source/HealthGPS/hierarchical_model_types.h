#pragma once
#include "interfaces.h"
#include "mapping.h"
#include "map2d.h"
#include "gender_value.h"

namespace hgps {

	using BaselineAdjustmentTable = Map2d<int, std::string, DoubleGenderValue>;

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
}