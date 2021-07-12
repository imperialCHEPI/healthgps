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

	void HierarchicalLinearModel::generate(RuntimeContext& context)
	{
		// TODO: Cache this information on the model construction.
		auto model_mapping = std::vector<MappingEntry>(
			context.mapping().begin(), context.mapping().end());
		if (!exclusions_.empty()) {
			for (auto& entry : exclusions_) {
				auto it = std::find_if(model_mapping.begin(), model_mapping.end(),
					[&entry](const auto& x) {
						return core::case_insensitive::equals(entry, x.name());
					});

				if (it != model_mapping.end()) {
					model_mapping.erase(it);
				}
			}
		}

		std::vector<MappingEntry> level_factors;
		std::unordered_map<int, std::vector<MappingEntry>> level_factors_cache;
		for (auto& entity : context.population()) {
			for (auto level = 1; level <= context.mapping().max_level(); level++) {
				if (level_factors_cache.contains(level)) {
					level_factors = level_factors_cache.at(level);
				}
				else {
					level_factors = context.mapping().at_level(level);
					level_factors_cache.emplace(level, level_factors);
				}

				generate_for_entity(context, entity, level, level_factors);
			}
		}
	}

	void HierarchicalLinearModel::generate_for_entity(RuntimeContext& context,
		Person& entity, int level, std::vector<MappingEntry>& level_factors)
	{
		auto level_info = levels_.at(level);

		// Residual Risk Factors Random Sampling 
		auto residualRiskFactors = std::unordered_map<std::string, double>();
		for (auto& item : level_factors) {
			auto row_idx = context.next_int(
				static_cast<int>(level_info.residual_distribution.rows() - 1));

			auto col_idx = level_info.variables.at(item.key());
			residualRiskFactors.emplace(item.key(), level_info.residual_distribution(row_idx, col_idx));
		}

		// The Stochastic Component of The Risk Factors
		auto stoch_comp_factors = std::unordered_map<std::string, double>();
		for (auto& item : level_factors) {
			if (!exclusions_.empty() &&
				std::find(exclusions_.begin(), exclusions_.end(), item.key()) != exclusions_.end()) {
				continue;
			}

			auto sum = 0.0;
			auto row_idx = level_info.variables.at(item.key());
			for (int j = 0; j < level_factors.size(); j++) {
				sum += level_info.transition(row_idx, j) * residualRiskFactors[item.key()];
			}

			stoch_comp_factors.emplace(item.key(), sum);
		}

		// The Deterministic Risk Factors
		auto determ_risk_factors = std::unordered_map<std::string, double>();
		determ_risk_factors.emplace("intercept", 1.0); // Need a better way.
		for (auto& item : context.mapping()) {
			if (item.level() >= level || (!exclusions_.empty() &&
				std::find(exclusions_.begin(), exclusions_.end(), item.key()) != exclusions_.end())) {
				continue;
			}

			determ_risk_factors.emplace(item.key(), entity.get_risk_factor_value(item.entity_key()));
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

	void DynamicHierarchicalLinearModel::generate_for_entity(RuntimeContext& context,
		Person& entity, int level, std::vector<MappingEntry>& level_factors) {

		throw std::logic_error(
			"DynamicHierarchicalLinearModel.generate_for_entity function not implemented.");
	}
}
