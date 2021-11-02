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
		double residuals_standard_deviation;
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

	struct BaselineAdjustment final {
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

	struct HierarchicalLinearModelDefinition final {
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

	struct FactorDynamicEquation {
		std::string name;
		std::map<std::string, double> coefficients;
		double residuals_standard_deviation;
	};

	struct AgeGroupGenderEquation {
		core::IntegerInterval age_group;
		std::map<std::string, FactorDynamicEquation> male;
		std::map<std::string, FactorDynamicEquation> female;
	};

	class LiteHierarchicalModelDefinition final {
	public:
		LiteHierarchicalModelDefinition() = delete;
		LiteHierarchicalModelDefinition(
			std::map<core::IntegerInterval, AgeGroupGenderEquation>&& equations,
			std::map<std::string, std::string>&& variables)
			: equations_{ std::move(equations) }, variables_{ std::move(variables) }
		{
			if (equations_.empty()) {
				throw std::invalid_argument("The model definition equations must not be empty.");
			}
		}

		const std::map<std::string, std::string>& variables() const noexcept {
			return variables_;
		}

		const AgeGroupGenderEquation& at(const int& age) const {
			for (auto& entry : equations_) {
				if (entry.first.contains(age)) {
					return entry.second;
				}
			}

			if (age < equations_.begin()->first.lower()) {
				return equations_.begin()->second;
			}

			return equations_.rbegin()->second;
		}

	private:
		std::map<core::IntegerInterval, AgeGroupGenderEquation> equations_;
		std::map<std::string, std::string> variables_;
	};
}