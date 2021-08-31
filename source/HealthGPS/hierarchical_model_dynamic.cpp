#include "hierarchical_model_dynamic.h"
#include "runtime_context.h"
#include "HealthGPS.Core\string_util.h"

namespace hgps {

	DynamicHierarchicalLinearModel::DynamicHierarchicalLinearModel(
		std::unordered_map<std::string, LinearModel>&& models,
		std::map<int, HierarchicalLevel>&& levels)
		: models_{ models }, levels_{ levels } {}
	
	HierarchicalModelType DynamicHierarchicalLinearModel::type() const noexcept {
		return HierarchicalModelType::Dynamic;
	}

	std::string DynamicHierarchicalLinearModel::name() const noexcept {
		return "Dynamic";
	}

	void DynamicHierarchicalLinearModel::generate_risk_factors(RuntimeContext& context) {
		std::vector<MappingEntry> level_factors;
		std::unordered_map<int, std::vector<MappingEntry>> level_factors_cache;
		for (auto& entity : context.population()) {
			// Newborns only
			if (entity.age > 0) {
				return;
			}

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

	void DynamicHierarchicalLinearModel::update_risk_factors(RuntimeContext& context) {
		std::vector<MappingEntry> level_factors;
		std::unordered_map<int, std::vector<MappingEntry>> level_factors_cache;
		for (auto& entity : context.population()) {
			// Ignore if dead, newborn risk factors must be generated, not updated!
			if (!entity.is_active() || entity.age == 0) {
				continue;
			}

			// Get risk factor for individual
			auto current_risk_factors = get_current_risk_factors(
				context.mapping(), entity, context.time_now());

			auto next_risk_factors = get_next_risk_factors(
				context.mapping(), entity, context.time_now());

			// Update risk factors for individual
			for (auto level = 1; level <= context.mapping().max_level(); level++) {
				if (level_factors_cache.contains(level)) {
					level_factors = level_factors_cache.at(level);
				}
				else {
					level_factors = context.mapping().at_level(level);
					level_factors_cache.emplace(level, level_factors);
				}

				update_for_entity(context, entity, level, level_factors,
					current_risk_factors, next_risk_factors);
			}
		}
	}

	void DynamicHierarchicalLinearModel::generate_for_entity(RuntimeContext& context,
		Person& entity, int level, std::vector<MappingEntry>& level_factors)
	{
		auto level_info = levels_.at(level);

		// Residual Risk Factors Random Sampling 
		auto residualRiskFactors = std::unordered_map<std::string, double>();
		for (auto& item : level_factors) {
			// next_int returns a value in range [0, maximum]
			auto row_idx = context.next_int(static_cast<int>(level_info.residual_distribution.rows() - 1));

			auto col_idx = level_info.variables.at(item.key());
			residualRiskFactors.emplace(item.key(), level_info.residual_distribution(row_idx, col_idx));
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
		// The Deterministic Risk Factors
		auto determ_risk_factors = std::unordered_map<std::string, double>();
		determ_risk_factors.emplace("intercept", entity.get_risk_factor_value("intercept"));
		for (auto& item : context.mapping()) {
			if (item.level() < level) {
				if (item.is_dynamic_factor()) {
					determ_risk_factors.emplace(item.key(), context.time_now());
				}
				else {
					determ_risk_factors.emplace(item.key(), entity.get_risk_factor_value(item.entity_key()));
				}
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

	void DynamicHierarchicalLinearModel::update_for_entity(RuntimeContext& context,
		Person& entity, const int& level, const std::vector<MappingEntry>& level_factors,
		std::map<std::string, double>& current_risk_factors,
		std::map<std::string, double>& next_risk_factors)
	{
		auto level_info = levels_.at(level);
		
		// Current deterministic components of risk factors
		auto current_determ_comp_factors = std::unordered_map<std::string, double>();
		for (const auto& item : level_factors) {
			auto sum = 0.0;
			for (const auto& coeff : models_.at(item.key()).coefficients) {
				sum += coeff.second.value * current_risk_factors.at(coeff.first);
			}

			current_determ_comp_factors.emplace(item.key(), sum);
		}

		// Current stochastic components of risk factors
		auto current_stoch_comp_factors = std::unordered_map<std::string, double>();
		for (const auto& item : level_factors) {
			auto stoch_comp = current_risk_factors.at(item.key()) - current_determ_comp_factors.at(item.key());
			current_stoch_comp_factors.emplace(item.key(), stoch_comp);
		}

		// Current independent component of risk factors
		auto current_independent_comp_factors = std::unordered_map<std::string, double>();
		for (const auto& item : level_factors) {
			auto row_sum = 0.0;
			auto row_idx = level_info.variables.at(item.key());
			for (int k = 0; k < level_factors.size(); k++) {
				row_sum += level_info.inverse_transition(row_idx, k) * current_stoch_comp_factors.at(item.key());
			}

			current_independent_comp_factors.emplace(item.key(), row_sum);
		}

		// Next independent component of risk factors: We need to update this dynamic
		auto next_independent_comp_factors = std::unordered_map<std::string, double>();
		for (const auto& factor : current_independent_comp_factors) {
			next_independent_comp_factors.emplace(factor.first, factor.second);
		}

		// Next stochastic component of risk factors 
		auto next_stoch_comp_factors = std::unordered_map<std::string, double>();
		for (const auto& item : level_factors) {
			auto row_sum = 0.0;
			auto row_idx = level_info.variables.at(item.key());
			for (int k = 0; k < level_factors.size(); k++) {
				row_sum += level_info.transition(row_idx, k) * next_independent_comp_factors.at(item.key());
			}

			next_stoch_comp_factors.emplace(item.key(), row_sum);
		}

		// Next deterministic components of risk factors
		auto next_determ_comp_factors = std::unordered_map<std::string, double>();
		for (const auto& item : level_factors) {
			auto sum = 0.0;
			for (const auto& coeff : models_.at(item.key()).coefficients) {
				sum += coeff.second.value * next_risk_factors.at(coeff.first);
			}

			next_determ_comp_factors.emplace(item.key(), sum);
		}

		// Update individual
		for (const auto& item : level_factors) {
			auto total_value = next_determ_comp_factors.at(item.key()) + next_stoch_comp_factors.at(item.key());
			entity.risk_factors[item.key()] = total_value;
		}
	}

	std::map<std::string, double> DynamicHierarchicalLinearModel::get_next_risk_factors(
		const HierarchicalMapping& mapping, Person& entity, int time_year) const {
		auto entity_risk_factors = std::map<std::string, double>();
		entity_risk_factors.emplace("intercept", entity.get_risk_factor_value("intercept"));
		for (const auto& factor : mapping) {
			if (factor.is_dynamic_factor()) {
				entity_risk_factors.emplace(factor.key(), time_year);
			}
			else {
				entity_risk_factors.emplace(factor.key(), entity.get_risk_factor_value(factor.key()));
			}
		}

		return entity_risk_factors;
	}

	std::map<std::string, double> DynamicHierarchicalLinearModel::get_current_risk_factors(
		const HierarchicalMapping& mapping, Person& entity, int time_year) const {
		auto entity_risk_factors = std::map<std::string, double>();
		entity_risk_factors.emplace("intercept", entity.get_previous_risk_factor_value("intercept"));
		for (const auto& factor : mapping) {
			if (factor.is_dynamic_factor()) {
				entity_risk_factors.emplace(factor.key(), time_year - 1);
			}
			else {
				entity_risk_factors.emplace(factor.key(), entity.get_previous_risk_factor_value(factor.key()));
			}
		}

		return entity_risk_factors;
	}
}
