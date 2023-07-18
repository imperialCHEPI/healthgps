#include "energy_balance_model.h"
#include "runtime_context.h"

namespace hgps {

EnergyBalanceModel::EnergyBalanceModel(
    const std::unordered_map<core::Identifier, double> &energy_equation,
    const std::unordered_map<core::Identifier, std::pair<double, double>> &nutrient_ranges,
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations,
    const std::unordered_map<core::Identifier, double> &food_prices,
    const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height)
    : energy_equation_{energy_equation}, nutrient_ranges_{nutrient_ranges},
      nutrient_equations_{nutrient_equations}, food_prices_{food_prices},
      age_mean_height_{age_mean_height} {

    if (energy_equation_.empty()) {
        throw std::invalid_argument("Energy equation mapping is empty");
    }
    if (nutrient_ranges_.empty()) {
        throw std::invalid_argument("Nutrient range mapping is empty");
    }
    if (nutrient_equations_.empty()) {
        throw std::invalid_argument("Nutrient equation mapping is empty");
    }
    if (food_prices_.empty()) {
        throw std::invalid_argument("Food price mapping is empty");
    }
    if (age_mean_height_.empty()) {
        throw std::invalid_argument("Age mean height mapping is empty");
    }
}

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

        [[maybe_unused]] double energy_intake = 0.0;
        std::unordered_map<core::Identifier, double> nutrient_intakes;

        // Initialise nutrients to zero.
        for (const auto &coefficient : energy_equation_) {
            nutrient_intakes[coefficient.first] = 0.0;
        }

        // Compute nutrient intakes from food intakes.
        for (const auto &equation : nutrient_equations_) {
            // TODO: until carbs is added to risk factors (should use at, not [])
            // double food_intake = current_risk_factors.at(equation.first);
            double food_intake = current_risk_factors[equation.first];

            for (const auto &coefficient : equation.second) {
                double delta_nutrient = food_intake * coefficient.second;
                nutrient_intakes[coefficient.first] += delta_nutrient;
            }
        }

        // Compute energy intake from nutrient intakes.
        for (const auto &coefficient : energy_equation_) {
            double delta_energy = nutrient_intakes[coefficient.first] * coefficient.second;
            energy_intake += delta_energy;
        }

        // TODO: model after energy intake stage
    }
}

double EnergyBalanceModel::bounded_nutrient_value(const core::Identifier &nutrient,
                                                  double value) const {
    auto &range = nutrient_ranges_.at(nutrient);
    if (value < range.first) {
        return range.first;
    }
    if (value > range.second) {
        return range.second;
    }
    return value;
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
    std::unordered_map<core::Identifier, double> energy_equation,
    std::unordered_map<core::Identifier, std::pair<double, double>> nutrient_ranges,
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
    std::unordered_map<core::Identifier, double> food_prices,
    std::unordered_map<core::Gender, std::vector<double>> age_mean_height)
    : energy_equation_{std::move(energy_equation)}, nutrient_ranges_{std::move(nutrient_ranges)},
      nutrient_equations_{std::move(nutrient_equations)}, food_prices_{std::move(food_prices)},
      age_mean_height_{std::move(age_mean_height)} {

    if (energy_equation_.empty()) {
        throw std::invalid_argument("Energy equation mapping is empty");
    }
    if (nutrient_ranges_.empty()) {
        throw std::invalid_argument("Nutrient ranges mapping is empty");
    }
    if (nutrient_equations_.empty()) {
        throw std::invalid_argument("Nutrient equation mapping is empty");
    }
    if (food_prices_.empty()) {
        throw std::invalid_argument("Food prices mapping is empty");
    }
    if (age_mean_height_.empty()) {
        throw std::invalid_argument("Age mean height mapping is empty");
    }
}

std::unique_ptr<HierarchicalLinearModel> EnergyBalanceModelDefinition::create_model() const {
    return std::make_unique<EnergyBalanceModel>(
        energy_equation_, nutrient_ranges_, nutrient_equations_, food_prices_, age_mean_height_);
}

} // namespace hgps
