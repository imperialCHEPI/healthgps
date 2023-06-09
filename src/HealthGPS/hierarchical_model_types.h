#pragma once
#include "interfaces.h"
#include "mapping.h"
#include "gender_value.h"

namespace hgps {

	/// @brief Regression coefficients data type
	struct Coefficient {
		/// @brief The coefficient value
		double value{};

		/// @brief Associated p-value
		double pvalue{};

		/// @brief Associated t-value
		double tvalue{};

		/// @brief Associated standard error
		double std_error{};
	};

	/// @brief Defines a linear regression model data type
	struct LinearModel {
		/// @brief The model coefficients 
		std::unordered_map<core::Identifier, Coefficient> coefficients;

		/// @brief The residuals standard deviation value
		double residuals_standard_deviation;

		/// @brief The R squared value
		double rsquared{};
	};

	/// @brief Defines a hierarchical level data type
	struct HierarchicalLevel {

		/// @brief The level variables
		std::unordered_map<core::Identifier, int> variables;
		
		/// @brief The transition matrix
		core::DoubleArray2D transition;

		/// @brief The inverse transition matrix
		core::DoubleArray2D inverse_transition;

		/// @brief The residuals distribution
		core::DoubleArray2D residual_distribution;

		/// @brief The correlation matrix
		core::DoubleArray2D correlation;

		/// @brief The associated variance
		std::vector<double> variances;
	};

	/// @brief Defines the full hierarchical linear model data type
	class HierarchicalLinearModelDefinition final {
	public:
		HierarchicalLinearModelDefinition() = delete;
		/// @brief Initialises a new instance of the HierarchicalLinearModelDefinition class
		/// @param linear_models The linear regression models equations
		/// @param model_levels The hierarchical model levels definition
		/// @throws std::invalid_argument for empty regression models equations or hierarchical model levels
		HierarchicalLinearModelDefinition(
			std::unordered_map<core::Identifier, LinearModel>&& linear_models,
			std::map<int, HierarchicalLevel>&& model_levels)
			: models_{ std::move(linear_models) }, levels_{ std::move(model_levels) }
		{
			if (models_.empty()) {
				throw std::invalid_argument("The hierarchical model equations definition must not be empty");
			}

			if (levels_.empty()) {
				throw std::invalid_argument("The hierarchical model levels definition must not be empty");
			}
		}

		/// @brief Gets the hierarchical model's linear regression equations
		/// @return Linear regression equations
		const std::unordered_map<core::Identifier, LinearModel>& models() const noexcept {
			return models_;
		}

		/// @brief Gets the hierarchical model's levels definitions
		/// @return Hierarchical model levels
		const std::map<int, HierarchicalLevel>& levels() const noexcept {
			return levels_;
		}

	private:
		std::unordered_map<core::Identifier, LinearModel> models_;
		std::map<int, HierarchicalLevel> levels_;
	};

	/// @brief Defines a factor linear regression equation
	struct FactorDynamicEquation {
		/// @brief The factor name
		std::string name;

		/// @brief The regression model coefficients
		std::map<core::Identifier, double> coefficients{};

		/// @brief The residuals standard deviation
		double residuals_standard_deviation{};
	};

	/// @brief Define the age group dynamic equations data type
	struct AgeGroupGenderEquation {
		/// @brief The reference age group
		core::IntegerInterval age_group;

		/// @brief The males dynamic equations
		std::map<core::Identifier, FactorDynamicEquation> male{};

		/// @brief The females dynamic equations
		std::map<core::Identifier, FactorDynamicEquation> female{};
	};

	/// @brief Defines the lite hierarchical linear model data type
	class LiteHierarchicalModelDefinition final {
	public:
		LiteHierarchicalModelDefinition() = delete;

		/// @brief Initialises a new instance of the LiteHierarchicalModelDefinition class
		/// @param equations The linear regression equations
		/// @param variables The factors delta variables mapping
		/// @param boundary_percentage The boundary percentage to sample
		/// @throws std::invalid_argument for empty model equations definition
		LiteHierarchicalModelDefinition(
			std::map<core::IntegerInterval, AgeGroupGenderEquation>&& equations,
			std::map<core::Identifier, core::Identifier>&& variables, const double boundary_percentage = 0.05)
			: equations_{ std::move(equations) }, variables_{ std::move(variables) }
			, boundary_percentage_{ boundary_percentage } {

			if (equations_.empty()) {
				throw std::invalid_argument("The model equations definition must not be empty");
			}
		}

		/// @brief Gets the model factors' delta variables mapping
		/// @return Delta variables mapping
		const std::map<core::Identifier, core::Identifier>& variables() const noexcept {
			return variables_;
		}

		/// @brief Gets the linear regression equations for a given age
		/// @param age The target age
		/// @return The model equations
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

		/// @brief Gets the boundary percentage to sample
		/// @return Boundary percentage value
		const double& boundary_percentage() const {
			return boundary_percentage_;
		}

	private:
		std::map<core::IntegerInterval, AgeGroupGenderEquation> equations_;
		std::map<core::Identifier, core::Identifier> variables_;
		double boundary_percentage_;
	};

	/// @brief Defines the energy balance model data type
	class EnergyBalanceModelDefinition final {
	public:
		EnergyBalanceModelDefinition() = delete;

		/// @brief Initialises a new instance of the EnergyBalanceModelDefinition class
		/// @param equations The linear regression equations
		/// @param variables The factors delta variables mapping
		/// @param boundary_percentage The boundary percentage to sample
		/// @throws std::invalid_argument for empty model equations definition
		EnergyBalanceModelDefinition(
			std::map<core::IntegerInterval, AgeGroupGenderEquation>&& equations,
			std::map<core::Identifier, core::Identifier>&& variables, const double boundary_percentage = 0.05)
			: equations_{ std::move(equations) }, variables_{ std::move(variables) }
			, boundary_percentage_{ boundary_percentage } {

			if (equations_.empty()) {
				throw std::invalid_argument("The model equations definition must not be empty");
			}
		}

		/// @brief Gets the model factors' delta variables mapping
		/// @return Delta variables mapping
		const std::map<core::Identifier, core::Identifier>& variables() const noexcept {
			return variables_;
		}

		/// @brief Gets the linear regression equations for a given age
		/// @param age The target age
		/// @return The model equations
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

		/// @brief Gets the boundary percentage to sample
		/// @return Boundary percentage value
		const double& boundary_percentage() const {
			return boundary_percentage_;
		}

	private:
		std::map<core::IntegerInterval, AgeGroupGenderEquation> equations_;
		std::map<core::Identifier, core::Identifier> variables_;
		double boundary_percentage_;
	};
}
