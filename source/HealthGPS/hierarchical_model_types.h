#pragma once
#include "interfaces.h"
#include "mapping.h"
#include "map2d.h"
#include "gender_value.h"

namespace hgps {

	using FactorAdjustmentTable = Map2d<core::Gender, std::string, std::vector<double>>;

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
		BaselineAdjustment() = delete;
		BaselineAdjustment(FactorAdjustmentTable&& adjustment_table)
			: values{ std::move(adjustment_table) }
		{
			if (values.empty()) {
				throw std::invalid_argument(
					"The risk factors adjustment table must not be empty.");
			}
			else if (values.rows() != 2) {
				throw std::invalid_argument(
					"The risk factors adjustment definition must contain two tables.");
			}

			if (!values.contains(core::Gender::male)) {
				throw std::invalid_argument("Missing the required adjustment table for male.");
			}
			else if (!values.contains(core::Gender::female)) {
				throw std::invalid_argument("Missing the required adjustment table for female.");
			}
		}

		const FactorAdjustmentTable values{};
	};

	struct HierarchicalLinearModelDefinition final {
		HierarchicalLinearModelDefinition() = delete;
		HierarchicalLinearModelDefinition(
			std::unordered_map<std::string, LinearModel>&& linear_models,
			std::map<int, HierarchicalLevel>&& model_levels,
			BaselineAdjustment&& baseline_adjustment)
			: models{ std::move(linear_models) }, levels{ std::move(model_levels) },
			adjustments{ std::move(baseline_adjustment) } {}

		const std::unordered_map<std::string, LinearModel> models;
		const std::map<int, HierarchicalLevel> levels;
		const BaselineAdjustment adjustments;
	};

	struct FactorDynamicEquation {
		std::string name;
		std::map<std::string, double> coefficients{};
		double residuals_standard_deviation{};
	};

	struct AgeGroupGenderEquation {
		core::IntegerInterval age_group;
		std::map<std::string, FactorDynamicEquation> male{};
		std::map<std::string, FactorDynamicEquation> female{};
	};

	class LiteHierarchicalModelDefinition final {
	public:
		LiteHierarchicalModelDefinition() = delete;
		LiteHierarchicalModelDefinition(
			std::map<core::IntegerInterval, AgeGroupGenderEquation>&& equations,
			std::map<std::string, std::string>&& variables,
			BaselineAdjustment&& baseline_adjustment,
			const double boundary_percentage = 0.05)
			: equations_{ std::move(equations) }, variables_{ std::move(variables) },
			adjustments_{ std::move(baseline_adjustment) }, boundary_percentage_{ boundary_percentage } {

			if (equations_.empty()) {
				throw std::invalid_argument("The model definition equations must not be empty.");
			}
		}

		const std::map<std::string, std::string>& variables() const noexcept { return variables_; }

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

		const BaselineAdjustment& adjustments() const { return adjustments_; }
		const double& boundary_percentage() const { return boundary_percentage_; }

	private:
		std::map<core::IntegerInterval, AgeGroupGenderEquation> equations_;
		std::map<std::string, std::string> variables_;
		BaselineAdjustment adjustments_;
		double boundary_percentage_;
	};
}