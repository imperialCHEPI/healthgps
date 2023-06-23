#include "energy_balance_model.h"
#include "runtime_context.h"
#include <iostream>

namespace hgps {

EnergyBalanceModelDefinition::EnergyBalanceModelDefinition(
    std::vector<core::Identifier> &&nutrient_list,
    std::map<core::Identifier, std::map<core::Identifier, double>> &&nutrient_equations)
    : nutrient_list_{std::move(nutrient_list)}, nutrient_equations_{std::move(nutrient_equations)} {

    if (nutrient_list_.empty()) {
        throw std::invalid_argument("Nutrient list is empty");
    }

    if (nutrient_equations_.empty()) {
        throw std::invalid_argument("Nutrient equation mapping is empty");
    }
}

const std::vector<core::Identifier> &EnergyBalanceModelDefinition::nutrient_list() const noexcept {
    return nutrient_list_;
}

const std::map<core::Identifier, std::map<core::Identifier, double>> &
EnergyBalanceModelDefinition::nutrient_equations() const noexcept {
    return nutrient_equations_;
}

EnergyBalanceModel::EnergyBalanceModel(EnergyBalanceModelDefinition &definition)
    : definition_{definition} {}

HierarchicalModelType EnergyBalanceModel::type() const noexcept {
    return HierarchicalModelType::Dynamic;
}

const std::string &EnergyBalanceModel::name() const noexcept { return name_; }

void EnergyBalanceModel::generate_risk_factors([[maybe_unused]] RuntimeContext &context) {
    throw std::logic_error("EnergyBalanceModel::generate_risk_factors not yet implemented.");
}

void EnergyBalanceModel::update_risk_factors(RuntimeContext &context) {
    auto age_key = core::Identifier{"age"};
    for (auto &entity : context.population()) {
        // Ignore if inactive, newborn risk factors must be generated, not updated!
        if (!entity.is_active() || entity.age == 0) {
            continue;
        }

        auto current_risk_factors =
            get_current_risk_factors(context.mapping(), entity, context.time_now());

        // Model calibrated on previous year's age
        auto model_age = static_cast<int>(entity.age - 1);
        if (current_risk_factors.at(age_key) > model_age) {
            current_risk_factors.at(age_key) = model_age;
        }

        // Compute nutrients from food groups.
        auto nutrient_list = definition_.get().nutrient_list();
        auto nutrient_equations = definition_.get().nutrient_equations();

        // Initialise nutrients to zero.
        std::unordered_map<core::Identifier, double> nutrient_values;
        for (const core::Identifier &nutrient_name : nutrient_list) {
            nutrient_values[nutrient_name] = 0.0;
        }

        // Compute nutrient values from foods.
        for (const auto &equation : nutrient_equations) {
            const core::Identifier &food_name = equation.first;
            double food_value = current_risk_factors[food_name];

            for (const auto &coefficient : equation.second) {
                double delta_nutrient = food_value * coefficient.second;
                nutrient_values[coefficient.first] += delta_nutrient;
            }
        }

        // TODO: model after input stage
    }
}

std::map<core::Identifier, double>
EnergyBalanceModel::get_current_risk_factors(const HierarchicalMapping &mapping, Person &entity,
                                             int time_year) const {
    auto entity_risk_factors = std::map<core::Identifier, double>();
    entity_risk_factors.emplace(InterceptKey, entity.get_risk_factor_value(InterceptKey));
    for (const auto &factor : mapping) {
        if (factor.is_dynamic_factor()) {
            entity_risk_factors.emplace(factor.key(), time_year - 1);
        } else {
            entity_risk_factors.emplace(factor.key(), entity.get_risk_factor_value(factor.key()));
        }
    }

    return entity_risk_factors;
}
} // namespace hgps
