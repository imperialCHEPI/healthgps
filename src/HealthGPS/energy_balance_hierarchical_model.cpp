#include "energy_balance_hierarchical_model.h"
#include "runtime_context.h"

namespace hgps {

EnergyBalanceHierarchicalModel::EnergyBalanceHierarchicalModel(
    const std::map<core::IntegerInterval, AgeGroupGenderEquation> &equations,
    const std::map<core::Identifier, core::Identifier> &variables, double boundary_percentage)
    : equations_{equations}, variables_{variables}, boundary_percentage_{boundary_percentage} {

    if (equations_.empty()) {
        throw std::invalid_argument("The model equations definition must not be empty");
    }

    if (variables_.empty()) {
        throw std::invalid_argument("The model variables definition must not be empty");
    }
}

HierarchicalModelType EnergyBalanceHierarchicalModel::type() const noexcept {
    return HierarchicalModelType::Dynamic;
}

std::string EnergyBalanceHierarchicalModel::name() const noexcept { return "Dynamic"; }

void EnergyBalanceHierarchicalModel::generate_risk_factors(
    [[maybe_unused]] RuntimeContext &context) {
    throw std::logic_error(
        "EnergyBalanceHierarchicalModel::generate_risk_factors not yet implemented.");
}

void EnergyBalanceHierarchicalModel::update_risk_factors(RuntimeContext &context) {
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

        auto &equations = equations_at(model_age);
        if (entity.gender == core::Gender::male) {
            update_risk_factors_exposure(context, entity, current_risk_factors, equations.male);
        } else {
            update_risk_factors_exposure(context, entity, current_risk_factors, equations.female);
        }
    }
}

const AgeGroupGenderEquation &EnergyBalanceHierarchicalModel::equations_at(const int &age) const {
    for (auto &entry : equations_) {
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

void EnergyBalanceHierarchicalModel::update_risk_factors_exposure(
    RuntimeContext &context, Person &entity,
    const std::map<core::Identifier, double> &current_risk_factors,
    const std::map<core::Identifier, FactorDynamicEquation> &equations) {
    auto delta_comp_factors = std::unordered_map<core::Identifier, double>();
    for (auto level = 1; level <= context.mapping().max_level(); level++) {
        auto level_factors = context.mapping().at_level(level);
        for (const auto &factor : level_factors) {
            auto &factor_equation = equations.at(factor.key());

            auto original_value = entity.get_risk_factor_value(factor.key());
            auto delta_factor = 0.0;
            for (const auto &coeff : factor_equation.coefficients) {
                if (current_risk_factors.contains(coeff.first)) {
                    delta_factor += coeff.second * current_risk_factors.at(coeff.first);
                } else {
                    auto &factor_key = variables_.at(coeff.first);
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
EnergyBalanceHierarchicalModel::get_current_risk_factors(const HierarchicalMapping &mapping,
                                                         Person &entity) const {
    auto entity_risk_factors = std::map<core::Identifier, double>();
    entity_risk_factors.emplace(InterceptKey, entity.get_risk_factor_value(InterceptKey));
    for (const auto &factor : mapping) {
        entity_risk_factors.emplace(factor.key(), entity.get_risk_factor_value(factor.key()));
    }

    return entity_risk_factors;
}

double EnergyBalanceHierarchicalModel::sample_normal_with_boundary(Random &random, double mean,
                                                                   double standard_deviation,
                                                                   double boundary) const {
    auto candidate = random.next_normal(mean, standard_deviation);
    auto cap = boundary_percentage_ * boundary;
    return std::min(std::max(candidate, -cap), +cap);
}

LiteHierarchicalModelDefinition::LiteHierarchicalModelDefinition(
    std::map<core::IntegerInterval, AgeGroupGenderEquation> equations,
    std::map<core::Identifier, core::Identifier> variables, const double boundary_percentage)
    : equations_{std::move(equations)}, variables_{std::move(variables)},
      boundary_percentage_{boundary_percentage} {

    if (equations_.empty()) {
        throw std::invalid_argument("The model equations definition must not be empty");
    }

    if (variables_.empty()) {
        throw std::invalid_argument("The model variables definition must not be empty");
    }
}

std::unique_ptr<HierarchicalLinearModel> LiteHierarchicalModelDefinition::create_model() const {
    return std::make_unique<EnergyBalanceHierarchicalModel>(equations_, variables_,
                                                            boundary_percentage_);
}

} // namespace hgps
