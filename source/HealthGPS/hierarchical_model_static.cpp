#include "hierarchical_model_static.h"
#include "runtime_context.h"
#include "HealthGPS.Core\string_util.h"
#include "gender_table.h"

namespace hgps {
	StaticHierarchicalLinearModel::StaticHierarchicalLinearModel(HierarchicalLinearModelDefinition& definition) 
		: definition_{definition}{}

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
		std::vector<MappingEntry> level_factors;
		std::unordered_map<int, std::vector<MappingEntry>> level_factors_cache;
		auto newborn_age = 0u;
		for (auto& entity : context.population()) {
			if (entity.age > newborn_age) {
				continue;
			}

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

	void StaticHierarchicalLinearModel::adjust_risk_factors_with_baseline(RuntimeContext& context) {
		if (!definition_.adjustments.is_enabled) {
			return;
		}

		auto time_year = context.time_now();
		auto baseline_adjustments = std::map<std::string, DoubleGenderValue>{};

		auto gender_sum = std::map<std::string, std::map<core::Gender, double>>{};
		auto gender_count = std::map<std::string, std::map<core::Gender, int>>{};
		for (const auto& factor : definition_.adjustments.risk_factors) {
			gender_sum[factor][core::Gender::male] = 0.0;
			gender_sum[factor][core::Gender::female] = 0.0;

			gender_count[factor][core::Gender::male] = 0;
			gender_count[factor][core::Gender::female] = 0;

			baseline_adjustments.emplace(factor, DoubleGenderValue{});
		}

		for (const auto& entity : context.population()) {
			for (const auto& factor : definition_.adjustments.risk_factors) {
				gender_sum[factor][entity.gender] += entity.get_risk_factor_value(factor);
				gender_count[factor][entity.gender]++;
			}
		}

		auto risk_factor_count = 0;
		const auto& baseline_averages_at_time = definition_.adjustments.averages.row(time_year);
		for (const auto& factor : definition_.adjustments.risk_factors) {
			risk_factor_count = gender_count[factor][core::Gender::male];
			if (risk_factor_count > 0) {
				auto risk_factor_average = gender_sum[factor][core::Gender::male] / risk_factor_count;
				auto baseline_value = baseline_averages_at_time.at(factor).male;
				auto adjustment = baseline_value - risk_factor_average;
				baseline_adjustments.at(factor).male = adjustment;
			}

			risk_factor_count = gender_count[factor][core::Gender::female];
			if (risk_factor_count > 0) {
				auto risk_factor_average = gender_sum[factor][core::Gender::female] / risk_factor_count;
				auto baseline_value = baseline_averages_at_time.at(factor).female;
				auto adjustment = baseline_value - risk_factor_average;
				baseline_adjustments.at(factor).female = adjustment;
			}
		}

		for (auto& entity : context.population()) {
			for (const auto& factor : definition_.adjustments.risk_factors) {
				auto risk_factor_value = entity.get_risk_factor_value(factor);
				auto adjustment = baseline_adjustments.at(factor);
				if (entity.gender == core::Gender::male) {
					entity.risk_factors.at(factor) = risk_factor_value + adjustment.male;
				}
				else {
					entity.risk_factors.at(factor) = risk_factor_value + adjustment.female;
				}
			}
		}
	}

	void StaticHierarchicalLinearModel::generate_for_entity(RuntimeContext& context,
		Person& entity, int level, std::vector<MappingEntry>& level_factors)
	{
		const auto& level_info = definition_.levels.at(level);

		// Residual Risk Factors Random Sampling
		auto residual_risk_factors = std::map<std::string, double>();
		for (const auto& factor : level_factors) {
			auto row_idx = context.random().next_int(static_cast<int>(level_info.residual_distribution.rows() - 1));
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
			for (const auto& coeff : definition_.models.at(factor.key()).coefficients) {
				sum += coeff.second.value * determ_risk_factors[coeff.first];
			}

			determ_comp_factors.emplace(factor.key(), sum);
		}

		for (const auto& factor : level_factors) {
			auto total_value = determ_comp_factors.at(factor.key()) + stoch_comp_factors.at(factor.key());
			entity.risk_factors[factor.key()] = factor.get_bounded_value(total_value);
		}
	}
}