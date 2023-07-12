#include "energy_balance_model.h"
#include "runtime_context.h"

namespace hgps {

EnergyBalanceModel::EnergyBalanceModel(
    const std::unordered_map<core::Identifier, double> &energy_equation,
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations,
    const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height)
    : energy_equation_{energy_equation}, nutrient_equations_{nutrient_equations},
      age_mean_height_{age_mean_height} {

    if (energy_equation_.empty()) {
        throw std::invalid_argument("Energy equation mapping is empty");
    }

    if (nutrient_equations_.empty()) {
        throw std::invalid_argument("Nutrient equation mapping is empty");
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

        double energy_intake = 0.0;
        std::unordered_map<core::Identifier, double> nutrient_intakes;

        // Initialise nutrients to zero.
        for (const auto &coefficient : energy_equation_) {
            nutrient_intakes[coefficient.first] = 0.0;
        }

        // Compute nutrient intakes from food intakes.
        for (const auto &equation : nutrient_equations_) {
            double food_intake = current_risk_factors.at(equation.first);

            for (const auto &coefficient : equation.second) {
                double delta_nutrient = food_intake * coefficient.second;
                nutrient_intakes.at(coefficient.first) += delta_nutrient;
            }
        }

        // Compute energy intake from nutrient intakes.
        for (const auto &coefficient : energy_equation_) {
            double delta_energy = nutrient_intakes.at(coefficient.first) * coefficient.second;
            energy_intake += delta_energy;
        }

        // TODO: model after energy intake stage
    }
}

void EnergyBalanceModel::get_steady_state(Person &entity, double offset) {
    // TODO: DUMMY VALUES: Model state variables.
    const double CI_0 = 0.0;  // Initial carbohydrate intake.
    const double CI = 0.0;    // New carbohydrate intake.
    const double F_0 = 0.1;   // Initial body fat.h
    const double L_0 = 0.1;   // Initial lean tissue.
    const double EI_0 = 0.0;  // Initial energy intake.
    const double EI = 0.0;    // New energy intake.
    const double ECF_0 = 0.2; // Initial extracellular fluid.
    const double K = 0.0;     // Model intercept value.
    const double AT = 0.0;    // Adaptive thermogenesis.

    // Model parameters.
    const double rho_F = 39.5e3; // Energy content of body fat (kJ/kg).
    const double rho_L = 7.6e3;  // Energy content of lean tissue (kJ/kg).
    const double gamma_F = 13.0; // RMR vs body fat regression coefficients (kJ/kg/day).
    const double gamma_L = 92.0; // RMR vs lean tissue regression coefficients (kJ/kg/day).
    const double eta_F = 750.0;  // Body fat synthesis energy coefficient (kJ/kg).
    const double eta_L = 960.0;  // Lean tissue synthesis energy coefficient (kJ/kg).
    const double beta_TEF = 0.1; // TEF from energy intake (unitless).
    const double beta_AT = 0.14; // AT from energy intake (unitless).

    // Energy partitioning equation.
    const double C = 10.4 * rho_L / rho_F;
    const double p = C / (C + F_0);

    // Glycogen and water equations.
    const double G_0 = 0.5;
    const double k_G = CI_0 / (G_0 * G_0);
    const double G = sqrt(CI / k_G);
    const double W = 2.7 * G;

    // Extracellular fluid equation.
    const double xi_Na = 3000.0; // sodium from ECF changes (mg/L/day).
    const double xi_CI = 4000.0; // sodium from carbohydrate changes (mg/day).
    const double Na_b = 4000.0;
    const double Na_f = Na_b * EI / EI_0;
    const double Delta_Na_diet = Na_f - Na_b;
    const double ECF = ECF_0 + (Delta_Na_diet - xi_CI * (1.0 - CI / CI_0)) / xi_Na;

    const double delta_EI = EI - EI_0;
    const double delta_0 = 0.0; // TODO: profile.GetPhysicalActivityDelta();

    // First equation ax + by = e.
    const double a1 = p * rho_F;
    const double b1 = -(1.0 - p) * rho_L;
    const double c1 = p * rho_F * F_0 - (1 - p) * rho_L * L_0;

    // Second equation cx + dy = f.
    const double a2 = gamma_F + delta_0;
    const double b2 = gamma_L + delta_0;
    const double c2 =
        EI - K - offset - beta_TEF * delta_EI - beta_AT * delta_EI - delta_0 * (G + W + ECF);

    // Steady state fat and lean equations.
    const double steady_F = -(b1 * c2 - b2 * c1) / (a1 * b2 - a2 * b1);
    const double steady_L = -(c1 * a2 - c2 * a1) / (a1 * b2 - a2 * b1);

    const double x = p * eta_L / rho_L + (1.0 - p) * eta_F / rho_F;
    const double tau = rho_L * rho_F * (1.0 + x) /
                       ((gamma_F + delta_0) * (1.0 - p) * rho_L + (gamma_L + delta_0) * p * rho_F);

    const double targetDate = 365.0; // 1 year
    const double F = steady_F + (F_0 - steady_F) * exp(-targetDate * 1.0 / tau);
    const double L = steady_L + (L_0 - steady_L) * exp(-targetDate * 1.0 / tau);

    const double BW = F + L + G + W + ECF;

    const double EE = (K + offset + gamma_F * F + gamma_L * L + delta_0 * BW + beta_TEF * delta_EI +
                       AT + EI * x) /
                      (1 + x);

    // TODO: return
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
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
    std::unordered_map<core::Gender, std::vector<double>> age_mean_height)
    : energy_equation_{std::move(energy_equation)},
      nutrient_equations_{std::move(nutrient_equations)},
      age_mean_height_{std::move(age_mean_height)} {

    if (energy_equation_.empty()) {
        throw std::invalid_argument("Energy equation mapping is empty");
    }

    if (nutrient_equations_.empty()) {
        throw std::invalid_argument("Nutrient equation mapping is empty");
    }

    if (age_mean_height_.empty()) {
        throw std::invalid_argument("Age mean height mapping is empty");
    }
}

std::unique_ptr<HierarchicalLinearModel> EnergyBalanceModelDefinition::create_model() const {
    return std::make_unique<EnergyBalanceModel>(energy_equation_, nutrient_equations_,
                                                age_mean_height_);
}

} // namespace hgps
