#pragma once
#include "interfaces.h"
#include "mapping.h"
#include "gender_value.h"

namespace hgps {

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

	class HierarchicalLinearModelDefinition final {
	public:
		HierarchicalLinearModelDefinition() = delete;
		HierarchicalLinearModelDefinition(
			std::unordered_map<std::string, LinearModel>&& linear_models,
			std::map<int, HierarchicalLevel>&& model_levels)
			: models_{ std::move(linear_models) }, levels_{ std::move(model_levels) }{}

		const std::unordered_map<std::string, LinearModel>& models() const noexcept {
			return models_;
		}

		const std::map<int, HierarchicalLevel>& levels() const noexcept {
			return levels_;
		}

	private:
		std::unordered_map<std::string, LinearModel> models_;
		std::map<int, HierarchicalLevel> levels_;
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
			std::map<std::string, std::string>&& variables, const double boundary_percentage = 0.05)
			: equations_{ std::move(equations) }, variables_{ std::move(variables) }
			, boundary_percentage_{ boundary_percentage } {

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

		const double& boundary_percentage() const {
			return boundary_percentage_;
		}

	private:
		std::map<core::IntegerInterval, AgeGroupGenderEquation> equations_;
		std::map<std::string, std::string> variables_;
		double boundary_percentage_;
	};
}