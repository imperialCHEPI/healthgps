#include "hierarchical_model.h"
#include "HealthGPS.Core\string_util.h"

namespace hgps {
	HierarchicalLinearModel::HierarchicalLinearModel(
		std::vector<std::string>& exclusions,
		std::unordered_map<std::string, LinearModel>&& models,
		std::map<int, HierarchicalLevel>&& levels)
		: models_{ models }, levels_{ levels },
		exclusions_{ exclusions.begin(), exclusions.end() }
	{}

	HierarchicalModelType HierarchicalLinearModel::type() const {
		return HierarchicalModelType::Static;
	}

	std::string HierarchicalLinearModel::name() const {
		return "Static";
	}

	void HierarchicalLinearModel::generate(RuntimeContext& context) {
		for (auto& entity : context.population())
		{
			for (auto level = 1; level <= context.mapping().max_level(); level++) {
				auto entries = context.mapping().at_level(level);
				auto residualRiskFactors = std::vector<double>(entries.size());

				auto level_info = levels_.at(level);

				// Residual Risk Factors Random Sampling 
				for (int col = 0; col < entries.size(); col++) {
					auto row_idx = context.next_int(
						static_cast<int>(level_info.residual_distribution.rows() - 1));

					residualRiskFactors[col] = level_info.residual_distribution(row_idx, col);
				}

				// The Stochastic Component of The Risk Factors
				auto stoch_comp_factors = std::unordered_map<std::string, double>();
				for (int j = 0; j < entries.size(); j++) {

					auto it = std::find_if(entries.begin(), entries.end(), [&](const auto& element) {
						return core::case_insensitive::equals(level_info.variables[j], element.name()); });
					if (it == entries.end()) {
						continue;
					}

					auto col_idx = std::distance(entries.begin(), it);
					auto sum = 0.0;
					for (int k = 0; k < entries.size(); k++) {
						sum += level_info.transition(j, k) * residualRiskFactors[k];
					}

					stoch_comp_factors.emplace(entries[col_idx].name(), sum);
				}

				// The Deterministic Risk Factors
				auto determ_risk_factors = std::unordered_map<std::string, double>();
				determ_risk_factors.emplace("Intercept", 1.0); // Need a better way.
				for (auto& item : context.mapping()) {
					if (item.level() < level && 
						!core::case_insensitive::contains(exclusions_, item.name())) {

						determ_risk_factors.emplace(item.name(),
							entity.get_risk_factor_value(item.key()));
					}
				}

				// The Deterministic Components of Risk Factors
				auto determ_comp_factors = std::unordered_map<std::string, double>();
				for (auto& item : entries) {
					auto sum = 0.0;
					for (auto& coeff : models_.at(item.name()).coefficients) {
						sum += coeff.second.value * determ_risk_factors[coeff.first];
					}

					determ_comp_factors.emplace(item.name(), sum);
				}

				for (auto& factor : determ_comp_factors) {
					auto total_value = factor.second + stoch_comp_factors.at(factor.first);
					entity.risk_factors[factor.first] = total_value;
				}			
			}
		}
	}

	// ************ Dynamic Hierarchical Model ************

	DynamicHierarchicalLinearModel::DynamicHierarchicalLinearModel(
		std::vector<std::string>& exclusions,
		std::unordered_map<std::string, LinearModel>&& models,
		std::map<int, HierarchicalLevel>&& levels)
		: HierarchicalLinearModel(exclusions, std::move(models), std::move(levels))
	{}

	HierarchicalModelType DynamicHierarchicalLinearModel::type() const {
		return HierarchicalModelType::Dynamic;
	}

	std::string DynamicHierarchicalLinearModel::name() const {
		return "Dynamic";
	}

	void DynamicHierarchicalLinearModel::generate(RuntimeContext& context) {
		throw std::logic_error(
			"DynamicHierarchicalLinearModel.generate function not implemented.");
	}
}
