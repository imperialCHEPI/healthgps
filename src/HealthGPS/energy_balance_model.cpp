#include "energy_balance_model.h"
#include "runtime_context.h"

namespace hgps {

// Rsik factor keys.
const core::Identifier H_key{"height"};
const core::Identifier BW_key{"weight"};
const core::Identifier PAL_key{"physical_activity_level"};
const core::Identifier F_key{"body_fat"};
const core::Identifier L_key{"lean_tissue"};
const core::Identifier ECF_key{"extracellular_fluid"};
const core::Identifier CI_key{"carbohydrate"};

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
    for (auto &person : context.population()) {
        // Ignore if inactive, newborn risk factors must be generated, not updated!
        if (!person.is_active() || person.age == 0) {
            continue;
        }

        double energy_intake = 0.0;
        std::unordered_map<core::Identifier, double> nutrient_intakes;

        // Initialise nutrients to zero.
        for (const auto &coefficient : energy_equation_) {
            nutrient_intakes[coefficient.first] = 0.0;
        }

        // Compute nutrient intakes from food intakes.
        for (const auto &equation : nutrient_equations_) {
            double food_intake = person.get_risk_factor_value(equation.first);

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

        // // Model initial state.
        // const double H_0 = person.get_risk_factor_value(H_key);
        // const double BW_0 = person.get_risk_factor_value(BW_key);
        // const double PAL_0 = person.get_risk_factor_value(PAL_key);
        // const double F_0 = person.get_risk_factor_value(F_key);
        // const double L_0 = person.get_risk_factor_value(L_key);
        // const double ECF_0 = person.get_risk_factor_value(ECF_key);
        // const double CI_0 = person.get_risk_factor_value(CI_key);
        // const double G_0 = 0.5;
    }
}

void EnergyBalanceModel::get_steady_state(Person &person, double offset) {
    // Model initial state.
    const double H_0 = person.get_risk_factor_value(H_key);
    const double BW_0 = person.get_risk_factor_value(BW_key);
    const double PAL_0 = person.get_risk_factor_value(PAL_key);
    const double F_0 = person.get_risk_factor_value(F_key);
    const double L_0 = person.get_risk_factor_value(L_key);
    const double ECF_0 = person.get_risk_factor_value(ECF_key);
    const double CI_0 = person.get_risk_factor_value(CI_key);
    const double G_0 = 0.5;

    // TODO: Initial energy intake.
    const double EI_0 = 0.0;

    // TODO: Update carbohydrate intake.
    double CI = CI_0;

    // Update glycogen and water.
    double k_G = CI_0 / (G_0 * G_0);
    double G = sqrt(CI / k_G);
    double W = 2.7 * G;

    // TODO: Update energy intake.
    double EI = EI_0;

    // Update extracellular fluid.
    double Na_b = 4000.0;
    double Na_f = Na_b * EI / EI_0;
    double Delta_Na_diet = Na_f - Na_b;
    double ECF = ECF_0 + (Delta_Na_diet - xi_CI * (1.0 - CI / CI_0)) / xi_Na;

    // TODO: Update height.
    double H = H_0;

    // TODO: Update physical activity level.
    double PAL = PAL_0;

    // Update resting metabolic rate (Mifflin-St Jeor).
    double RMR = 9.99 * BW_0 + 6.25 * H * 100.0 - 4.92 * person.age;
    RMR += person.gender == core::Gender::male ? 5.0 : -161.0;
    RMR *= 4.184; // kcal to kJ

    double delta_0 = ((1.0 - beta_TEF) * PAL - 1.0) * RMR / BW_0;

    // Update thermic effect of food and adaptive thermogenesis.
    double TEF = beta_TEF * EI - EI_0;
    double AT = beta_AT * EI - EI_0;

    // TODO: Update intercept value.
    const double K = 0.0; // TODO: Intercept value.

    // Energy partitioning.
    const double C = 10.4 * rho_L / rho_F;
    const double p = C / (C + F_0);

    // First equation ax + by = e.
    double a1 = p * rho_F;
    double b1 = -(1.0 - p) * rho_L;
    double c1 = p * rho_F * F_0 - (1 - p) * rho_L * L_0;

    // Second equation cx + dy = f.
    double a2 = gamma_F + delta_0;
    double b2 = gamma_L + delta_0;
    double c2 = EI - offset - K - TEF - AT - delta_0 * (G + W + ECF);

    // Update body fat and lean tissue steady state.
    double steady_F = -(b1 * c2 - b2 * c1) / (a1 * b2 - a2 * b1);
    double steady_L = -(c1 * a2 - c2 * a1) / (a1 * b2 - a2 * b1);

    double x = p * eta_L / rho_L + (1.0 - p) * eta_F / rho_F;
    double tau = rho_L * rho_F * (1.0 + x) /
                 ((gamma_F + delta_0) * (1.0 - p) * rho_L + (gamma_L + delta_0) * p * rho_F);

    // Update body fat and lean tissue.
    double F = steady_F + (F_0 - steady_F) * exp(-365.0 / tau);
    double L = steady_L + (L_0 - steady_L) * exp(-365.0 / tau);

    // Update body weight.
    double BW = F + L + G + W + ECF;

    double delta_BW = delta_0 * BW;

    // Update energy expenditure.
    double EE = (offset + K + gamma_F * F + gamma_L * L + delta_BW + TEF + AT + EI * x) / (1.0 + x);

    // TODO: return
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
