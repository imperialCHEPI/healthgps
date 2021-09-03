#include "hierarchical_model_static.h"
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

	void StaticHierarchicalLinearModel::generate_risk_factors(RuntimeContext& context)
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

	void StaticHierarchicalLinearModel::update_risk_factors(RuntimeContext& context) {
		throw std::logic_error("StaticHierarchicalLinearModel.update not yet implemented.");
	}

	void StaticHierarchicalLinearModel::generate_for_entity(RuntimeContext& context,
		Person& entity, int level, std::vector<MappingEntry>& level_factors)
	{
		const auto level_info = levels_.at(level);

		// Residual Risk Factors Random Sampling
		auto residual_risk_factors = std::map<std::string, double>();
		for (const auto& factor : level_factors) {
			auto row_idx = context.next_int(static_cast<int>(level_info.residual_distribution.rows() - 1));
			auto col_idx = level_info.variables.at(factor.key());
			residual_risk_factors.emplace(factor.key(), level_info.residual_distribution(row_idx, col_idx));
		}

		// The Stochastic Component of The Risk Factors
		auto stoch_comp_factors = std::map<std::string, double>();
		for (const auto& factor_row : level_factors) {
			auto row_sum = 0.0;
			auto row_idx = level_info.variables.at(factor_row.key());
			for (const auto& factor_col : level_factors) {
				auto col_idx = level_info.variables.at(factor_col.key());
				row_sum += level_info.transition(row_idx, col_idx) * residual_risk_factors[factor_col.key()];
			}

			stoch_comp_factors.emplace(factor_row.key(), row_sum);
		}

		// The Deterministic Risk Factors
		auto determ_risk_factors = std::map<std::string, double>();
		determ_risk_factors.emplace("intercept", entity.get_risk_factor_value("intercept"));
		for (const auto& item : context.mapping()) {
			if (item.level() < level && !item.is_dynamic_factor()) {
				determ_risk_factors.emplace(item.key(), entity.get_risk_factor_value(item.entity_key()));
			}
		}

		// The Deterministic Components of Risk Factors
		auto determ_comp_factors = std::map<std::string, double>();
		for (const auto& factor : level_factors) {
			auto sum = 0.0;
			for (const auto& coeff : models_.at(factor.key()).coefficients) {
				sum += coeff.second.value * determ_risk_factors[coeff.first];
			}

			determ_comp_factors.emplace(factor.key(), sum);
		}

		for (const auto& factor : determ_comp_factors) {
			auto total_value = factor.second + stoch_comp_factors.at(factor.first);
			entity.risk_factors[factor.first] = total_value;
		}
	}
}