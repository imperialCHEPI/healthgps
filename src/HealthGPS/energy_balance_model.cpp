#include "energy_balance_model.h"
#include "runtime_context.h"

namespace hgps {

EnergyBalanceModel::EnergyBalanceModel(
    const std::vector<core::Identifier> &nutrient_list,
    const std::map<core::Identifier, std::map<core::Identifier, double>> &nutrient_equations)
    : nutrient_list_{nutrient_list}, nutrient_equations_{nutrient_equations} {}

HierarchicalModelType EnergyBalanceModel::type() const noexcept {
    return HierarchicalModelType::Dynamic;
}

std::string EnergyBalanceModel::name() const noexcept { return "Dynamic"; }

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
        const auto &nutrient_list = nutrient_list_.get();
        const auto &nutrient_equations = nutrient_equations_.get();

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

EnergyBalanceModelDefinition::EnergyBalanceModelDefinition(
    std::vector<core::Identifier> nutrient_list,
    std::map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations)
    : nutrient_list_{std::move(nutrient_list)}, nutrient_equations_{std::move(nutrient_equations)} {

    if (nutrient_list_.empty()) {
        throw std::invalid_argument("Nutrient list is empty");
    }

    if (nutrient_equations_.empty()) {
        throw std::invalid_argument("Nutrient equation mapping is empty");
    }
}

std::unique_ptr<HierarchicalLinearModel> EnergyBalanceModelDefinition::create_model() const {
    return std::make_unique<EnergyBalanceModel>(nutrient_list_, nutrient_equations_);
}

} // namespace hgps
