#include "dynamic_hierarchical_linear_model.h"
#include "runtime_context.h"

#include "HealthGPS.Core/exception.h"

#include <vector>

namespace hgps {

DynamicHierarchicalLinearModel::DynamicHierarchicalLinearModel(
    const RiskFactorSexAgeTable &expected,
    const std::map<core::IntegerInterval, AgeGroupGenderEquation> &equations,
    const std::map<core::Identifier, core::Identifier> &variables, double boundary_percentage)
    : RiskFactorAdjustableModel{expected}, equations_{equations}, variables_{variables},
      boundary_percentage_{boundary_percentage} {

    if (equations_.empty()) {
        throw core::HgpsException("The model equations definition must not be empty");
    }

    if (variables_.empty()) {
        throw core::HgpsException("The model variables definition must not be empty");
    }
}

RiskFactorModelType DynamicHierarchicalLinearModel::type() const noexcept {
    return RiskFactorModelType::Dynamic;
}

std::string DynamicHierarchicalLinearModel::name() const noexcept { return "Dynamic"; }

void DynamicHierarchicalLinearModel::generate_risk_factors(RuntimeContext &context) {

    // Adjust risk factors such that mean sim value matches expected value.
    std::vector<core::Identifier> factor_keys;
    factor_keys.reserve(variables_.size());
    for (const auto &factor : variables_) {
        // factor.second contains the factor name.
        factor_keys.emplace_back(factor.second);
    }
    adjust_risk_factors(context, factor_keys);
}

void DynamicHierarchicalLinearModel::update_risk_factors(RuntimeContext &context) {
    auto age_key = core::Identifier{"age"};
    for (auto &entity : context.population()) {
        // Ignore if inactive, newborn risk factors must be generated, not updated!
        if (!entity.is_active() || entity.age == 0) {
            continue;
        }

        auto current_risk_factors = get_current_risk_factors(context.mapping(), entity);

        // Model calibrated on previous year's age
        auto model_age = static_cast<int>(entity.age - 1);
        if (current_risk_factors.at(age_key) > model_age) {
            current_risk_factors.at(age_key) = model_age;
        }

        const auto &equations = equations_at(model_age);
        if (entity.gender == core::Gender::male) {
            update_risk_factors_exposure(context, entity, current_risk_factors, equations.male);
        } else {
            update_risk_factors_exposure(context, entity, current_risk_factors, equations.female);
        }
    }

    // Adjust risk factors such that mean sim value matches expected value.
    std::vector<core::Identifier> factor_keys;
    factor_keys.reserve(variables_.size());
    for (const auto &factor : variables_) {
        // factor.second contains the factor name.
        factor_keys.emplace_back(factor.second);
    }
    adjust_risk_factors(context, factor_keys);
}

const AgeGroupGenderEquation &DynamicHierarchicalLinearModel::equations_at(int age) const {
    for (const auto &entry : equations_) {
        if (entry.first.contains(age)) {
            // If there is an equation for the age, return it.
            return entry.second;
        }
    }
    if (age < equations_.begin()->first.lower()) {
        // Else if the age is below the first equation, return the first equation.
        return equations_.begin()->second;
    }
    // Else if the age is above the last equation, return the last equation.
    return equations_.rbegin()->second;
}

void DynamicHierarchicalLinearModel::update_risk_factors_exposure(
    RuntimeContext &context, Person &entity,
    const std::map<core::Identifier, double> &current_risk_factors,
    const std::map<core::Identifier, FactorDynamicEquation> &equations) {
    auto delta_comp_factors = std::unordered_map<core::Identifier, double>();
    for (auto level = 1; level <= context.mapping().max_level(); level++) {
        auto level_factors = context.mapping().at_level(level);
        for (const auto &factor : level_factors) {
            const auto &factor_equation = equations.at(factor.key());

            auto original_value = entity.get_risk_factor_value(factor.key());
            auto delta_factor = 0.0;
            for (const auto &coeff : factor_equation.coefficients) {
                if (current_risk_factors.contains(coeff.first)) {
                    delta_factor += coeff.second * current_risk_factors.at(coeff.first);
                } else {
                    const auto &factor_key = variables_.at(coeff.first);
                    delta_factor += coeff.second * delta_comp_factors.at(factor_key);
                }
            }

            // Intervention: "-1" as we want to retrieve data from last year
            delta_factor = context.scenario().apply(
                context.random(), entity, context.time_now() - 1, factor.key(), delta_factor);
            auto factor_stdev = factor_equation.residuals_standard_deviation;
            delta_factor +=
                sample_normal_with_boundary(context.random(), 0.0, factor_stdev, original_value);
            delta_comp_factors.emplace(factor.key(), delta_factor);

            auto updated_value = entity.risk_factors.at(factor.key()) + delta_factor;
            entity.risk_factors.at(factor.key()) = factor.get_bounded_value(updated_value);
        }
    }
}

std::map<core::Identifier, double>
DynamicHierarchicalLinearModel::get_current_risk_factors(const HierarchicalMapping &mapping,
                                                         Person &entity) {
    auto entity_risk_factors = std::map<core::Identifier, double>();
    entity_risk_factors.emplace(InterceptKey, entity.get_risk_factor_value(InterceptKey));
    for (const auto &factor : mapping) {
        entity_risk_factors.emplace(factor.key(), entity.get_risk_factor_value(factor.key()));
    }

    return entity_risk_factors;
}

double DynamicHierarchicalLinearModel::sample_normal_with_boundary(Random &random, double mean,
                                                                   double standard_deviation,
                                                                   double boundary) const {
    auto candidate = random.next_normal(mean, standard_deviation);
    auto cap = boundary_percentage_ * boundary;
    return std::min(std::max(candidate, -cap), +cap);
}

DynamicHierarchicalLinearModelDefinition::DynamicHierarchicalLinearModelDefinition(
    RiskFactorSexAgeTable expected,
    std::map<core::IntegerInterval, AgeGroupGenderEquation> equations,
    std::map<core::Identifier, core::Identifier> variables, const double boundary_percentage)
    : RiskFactorAdjustableModelDefinition{std::move(expected)}, equations_{std::move(equations)},
      variables_{std::move(variables)}, boundary_percentage_{boundary_percentage} {

    if (equations_.empty()) {
        throw core::HgpsException("The model equations definition must not be empty");
    }

    if (variables_.empty()) {
        throw core::HgpsException("The model variables definition must not be empty");
    }
}

std::unique_ptr<RiskFactorModel> DynamicHierarchicalLinearModelDefinition::create_model() const {
    const auto &expected = get_risk_factor_expected();
    return std::make_unique<DynamicHierarchicalLinearModel>(expected, equations_, variables_,
                                                            boundary_percentage_);
}

} // namespace hgps
