#include "hierarchical_model.h"
#include "runtime_context.h"
#include "HealthGPS.Core\string_util.h"

namespace hgps {
	StaticHierarchicalLinearModel::StaticHierarchicalLinearModel(
		std::unordered_map<std::string, LinearModel>&& models,
		std::map<int, HierarchicalLevel>&& levels)
		: models_{ models }, levels_{ levels }
	{}

	HierarchicalModelType StaticHierarchicalLinearModel::type() const noexcept {
		return HierarchicalModelType::Static;
	}

	std::string StaticHierarchicalLinearModel::name() const noexcept {
		return "Static";
	}

	void StaticHierarchicalLinearModel::generate(RuntimeContext& context)
	{
		std::vector<MappingEntry> level_factors;
		std::unordered_map<int, std::vector<MappingEntry>> level_factors_cache;
		for (auto& entity : context.population()) {
			for (auto level = 1; level <= context.mapping().max_level(); level++) {
				if (level_factors_cache.contains(level)) {
					level_factors = level_factors_cache.at(level);
				}
				else {
					level_factors = context.mapping().at_level_without_dynamic(level);
					level_factors_cache.emplace(level, level_factors);
				}

				generate_for_entity(context, entity, level, level_factors);
			}
		}
	}

	void StaticHierarchicalLinearModel::generate_for_entity(RuntimeContext& context,
		Person& entity, int level, std::vector<MappingEntry>& level_factors)
	{
		auto level_info = levels_.at(level);

		// Residual Risk Factors Random Sampling 
		auto residualRiskFactors = std::unordered_map<std::string, double>();
		for (auto& item : level_factors) {
			// next_int returns a value in range [0, maximum]
			auto row_idx = context.next_int(
				static_cast<int>(level_info.residual_distribution.rows() - 1));

			auto col_idx = level_info.variables.at(item.key());
			residualRiskFactors.emplace(item.key(), 
				level_info.residual_distribution(row_idx, col_idx));
		}

		// The Stochastic Component of The Risk Factors
		auto stoch_comp_factors = std::unordered_map<std::string, double>();
		for (auto& item : level_factors) {
			auto row_sum = 0.0;
			auto row_idx = level_info.variables.at(item.key());
			for (int k = 0; k < level_factors.size(); k++) {
				row_sum += level_info.transition(row_idx, k) * residualRiskFactors[item.key()];
			}

			stoch_comp_factors.emplace(item.key(), row_sum);
		}

		// The Deterministic Risk Factors
		auto determ_risk_factors = std::unordered_map<std::string, double>();
		determ_risk_factors.emplace("intercept", 1.0); // Need a better way.
		for (auto& item : context.mapping()) {
			if (item.level() < level && !item.is_dynamic_factor()) {
				determ_risk_factors.emplace(item.key(),
					entity.get_risk_factor_value(item.entity_key()));
			}
		}

		// The Deterministic Components of Risk Factors
		auto determ_comp_factors = std::unordered_map<std::string, double>();
		for (auto& item : level_factors) {
			auto sum = 0.0;
			for (auto& coeff : models_.at(item.key()).coefficients) {
				sum += coeff.second.value * determ_risk_factors[coeff.first];
			}

			determ_comp_factors.emplace(item.key(), sum);
		}

		for (auto& factor : determ_comp_factors) {
			auto total_value = factor.second + stoch_comp_factors.at(factor.first);
			entity.risk_factors[factor.first] = total_value;
		}
	}

	// ************ Dynamic Hierarchical Model ************

	DynamicHierarchicalLinearModel::DynamicHierarchicalLinearModel(
		std::unordered_map<std::string, LinearModel>&& models,
		std::map<int, HierarchicalLevel>&& levels)
		: StaticHierarchicalLinearModel(std::move(models), std::move(levels))
	{}

	HierarchicalModelType DynamicHierarchicalLinearModel::type() const noexcept {
		return HierarchicalModelType::Dynamic;
	}

	std::string DynamicHierarchicalLinearModel::name() const noexcept {
		return "Dynamic";
	}

	void DynamicHierarchicalLinearModel::generate_for_entity(RuntimeContext& context,
		Person& entity, int level, std::vector<MappingEntry>& level_factors) {

		throw std::logic_error(
			"DynamicHierarchicalLinearModel.generate_for_entity function not implemented.");
	}
}
