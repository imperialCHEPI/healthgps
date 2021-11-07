#include "energy_balance_model.h"
#include "runtime_context.h"
#include <iostream>

namespace hgps {

	EnergyBalanceHierarchicalModel::EnergyBalanceHierarchicalModel(
		LiteHierarchicalModelDefinition& definition)
		: definition_{ definition } {}

	HierarchicalModelType EnergyBalanceHierarchicalModel::type() const noexcept {
		return HierarchicalModelType::Dynamic;
	}

	std::string EnergyBalanceHierarchicalModel::name() const noexcept {
		return "Dynamic";
	}

	void EnergyBalanceHierarchicalModel::generate_risk_factors(RuntimeContext& context) {
		throw std::logic_error("EnergyBalanceHierarchicalModel::generate_risk_factors not yet implemented.");
	}

	void EnergyBalanceHierarchicalModel::update_risk_factors(RuntimeContext& context) {
		std::vector<MappingEntry> level_factors;
		std::unordered_map<int, std::vector<MappingEntry>> level_factors_cache;
		for (auto& entity : context.population()) {
			// Ignore if inactive, newborn risk factors must be generated, not updated!
			if (!entity.is_active() || entity.age == 0) {
				continue;
			}

			auto current_risk_factors = get_current_risk_factors(
				context.mapping(), entity, context.time_now());

			// Model calibrated on previous year's age
			auto model_age = static_cast<int>(entity.age - 1);
			if (current_risk_factors.at("age") > model_age) {
				current_risk_factors.at("age") = model_age;
			}

			auto equations = definition_.at(model_age);
			if (entity.gender == core::Gender::male) {
				update_risk_factors_exposure(context, entity, current_risk_factors, equations.male);
			}
			else {
				update_risk_factors_exposure(context, entity, current_risk_factors, equations.female);
			}
		}
	}

	void EnergyBalanceHierarchicalModel::update_risk_factors_exposure(RuntimeContext& context, Person& entity,
		std::map<std::string, double>& current_risk_factors, std::map<std::string, FactorDynamicEquation>& equations)
	{
		auto local_age = entity.age;
		if (current_risk_factors.contains("age")) {
			local_age = static_cast<unsigned int>(current_risk_factors.at("age"));
		}

		auto delta_comp_factors = std::unordered_map<std::string, double>();
		auto adjustments = definition_.adjustments().values.row(entity.gender);
		for (auto level = 1; level <= context.mapping().max_level(); level++) {
			auto level_factors = context.mapping().at_level(level);
			for (const auto& factor : level_factors) {
				auto factor_equation = equations.at(factor.key());

				auto original_value = entity.get_risk_factor_value(factor.key());
				auto delta_factor = adjustments.at(factor.key()).at(local_age);
				for (const auto& coeff : factor_equation.coefficients) {
					if (current_risk_factors.contains(coeff.first)) {
						delta_factor += coeff.second * current_risk_factors.at(coeff.first);
					}
					else {
						auto factor_key = definition_.variables().at(coeff.first);
						delta_factor += coeff.second * delta_comp_factors.at(factor_key);
					}
				}

				// Intervention: "-1" as we want to retrieve data from last year
				delta_factor = context.scenario().apply(entity, context.time_now() - 1, factor.key(), delta_factor);

				auto boundary = original_value + delta_factor;
				auto factor_stdev = factor_equation.residuals_standard_deviation;
				delta_factor += sample_normal_with_boundary(context.random(), 0.0, factor_stdev, boundary);
				delta_comp_factors.emplace(factor.key(), delta_factor);

				auto updated_value = entity.risk_factors.at(factor.key()) + delta_factor;
				entity.risk_factors.at(factor.key()) = factor.get_bounded_value(updated_value);
			}
		}
	}

	std::map<std::string, double> EnergyBalanceHierarchicalModel::get_current_risk_factors(
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

	double EnergyBalanceHierarchicalModel::sample_normal_with_boundary(
		Random& random, double mean, double standard_deviation, double boundary) const {
		auto absolute_boundary = std::abs(boundary);
		auto candidate = random.next_normal(mean, standard_deviation);
		auto percentage = definition_.boundary_percentage();
		auto cap = percentage * boundary;
		return std::min(std::max(candidate, -cap), +cap);
	}
}
