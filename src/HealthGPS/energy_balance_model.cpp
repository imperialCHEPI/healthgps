#include "energy_balance_model.h"
#include "runtime_context.h"

#include <algorithm>

namespace hgps {

// Risk factor keys.
const core::Identifier H_key{"height"};
const core::Identifier BW_key{"weight"};
const core::Identifier PAL_key{"physical_activity_level"};
const core::Identifier RMR_key{"resting_metabolic_rate"};
const core::Identifier F_key{"body_fat"};
const core::Identifier L_key{"lean_tissue"};
const core::Identifier ECF_key{"extracellular_fluid"};
const core::Identifier G_key{"glycogen"};
const core::Identifier W_key{"water"};
const core::Identifier EE_key{"energy_expenditure"};
const core::Identifier EI_key{"energy_intake"};
const core::Identifier CI_key{"carbohydrate"};

EnergyBalanceModel::EnergyBalanceModel(
    const std::unordered_map<core::Identifier, double> &energy_equation,
    const std::unordered_map<core::Identifier, std::pair<double, double>> &nutrient_ranges,
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations,
    const std::unordered_map<core::Identifier, std::optional<double>> &food_prices,
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
    hgps::Population &population = context.population();
    double mean_sim_body_weight = 0.0;
    double mean_adjustment_coefficient = 0.0;

    // TODO: Compute target body weight.
    const float target_BW = 100.0;

    // Trial run.
    for (auto &person : population) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        // Simulate person and compute adjustment coefficient.
        SimulatePersonState state = simulate_person(person, 0.0);
        mean_sim_body_weight += state.BW;
        mean_adjustment_coefficient += state.adjust;
    }

    // Compute model adjustment term.
    const size_t population_size = population.current_active_size();
    mean_sim_body_weight /= population_size;
    mean_adjustment_coefficient /= population_size;
    double shift = (target_BW - mean_sim_body_weight) / mean_adjustment_coefficient;

    // Final run.
    for (auto &person : population) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        // TODO: Simulate person and update risk factors.
        simulate_person(person, shift);
        // SimulatePersonState state = simulate_person(person, shift);
        // person.risk_factors[H_key] = state.H;
        // person.risk_factors[BW_key] = state.BW;
        // person.risk_factors[PAL_key] = state.PAL;
        // person.risk_factors[RMR_key] = state.RMR;
        // person.risk_factors[F_key] = state.F;
        // person.risk_factors[L_key] = state.L;
        // person.risk_factors[ECF_key] = state.ECF;
        // person.risk_factors[G_key] = state.G;
        // person.risk_factors[W_key] = state.W;
        // person.risk_factors[EE_key] = state.EE;
        // person.risk_factors[EI_key] = state.EI;
    }
}

SimulatePersonState EnergyBalanceModel::simulate_person(Person &person, double shift) const {
    // Initial simulated person state.
    const double H_0 = person.get_risk_factor_value(H_key);
    const double BW_0 = person.get_risk_factor_value(BW_key);
    const double PAL_0 = person.get_risk_factor_value(PAL_key);
    const double F_0 = person.get_risk_factor_value(F_key);
    const double L_0 = person.get_risk_factor_value(L_key);
    const double ECF_0 = person.get_risk_factor_value(ECF_key);
    const double G_0 = person.get_risk_factor_value(G_key);
    const double EI_0 = person.get_risk_factor_value(EI_key);
    const double CI_0 = person.get_risk_factor_value(CI_key);

    // TODO: Compute height.
    double H = H_0;

    // TODO: Compute physical activity level.
    double PAL = PAL_0;

    // Compute energy intake and carbohydrate intake.
    auto nutrient_intakes = compute_nutrient_intakes(person);
    double EI = compute_EI(nutrient_intakes);
    double CI = nutrient_intakes.at(CI_key);

    // Compute glycogen and water.
    double G = compute_G(CI, CI_0, G_0);
    double W = 2.7 * G;

    // Compute extracellular fluid.
    double ECF = compute_ECF(EI, EI_0, CI, CI_0, ECF_0);

    // Compute resting metabolic rate (Mifflin-St Jeor).
    double RMR = compute_RMR(BW_0, H, person.age, person.gender);

    // Compute delta.
    double delta = ((1.0 - beta_TEF) * PAL - 1.0) * RMR / BW_0;

    // Compute thermic effect of food and adaptive thermogenesis.
    double TEF = beta_TEF * EI - EI_0;
    double AT = beta_AT * EI - EI_0;

    // TODO: Compute intercept value.
    const double K = 0.0; // TODO: Intercept value.

    // Energy partitioning.
    const double C = 10.4 * rho_L / rho_F;
    const double p = C / (C + F_0);

    // First equation ax + by = e.
    double a1 = p * rho_F;
    double b1 = -(1.0 - p) * rho_L;
    double c1 = p * rho_F * F_0 - (1 - p) * rho_L * L_0;

    // Second equation cx + dy = f.
    double a2 = gamma_F + delta;
    double b2 = gamma_L + delta;
    double c2 = EI - shift - K - TEF - AT - delta * (G + W + ECF);

    // Compute body fat and lean tissue steady state.
    double steady_F = -(b1 * c2 - b2 * c1) / (a1 * b2 - a2 * b1);
    double steady_L = -(c1 * a2 - c2 * a1) / (a1 * b2 - a2 * b1);

    double x = p * eta_L / rho_L + (1.0 - p) * eta_F / rho_F;

    // Compute time constant.
    double tau = rho_L * rho_F * (1.0 + x) /
                 ((gamma_F + delta) * (1.0 - p) * rho_L + (gamma_L + delta) * p * rho_F);

    // Compute body fat and lean tissue.
    double F = steady_F - (steady_F - F_0) * exp(-365.0 / tau);
    double L = steady_L - (steady_L - L_0) * exp(-365.0 / tau);

    // Compute adjustment coefficient.
    double adjust = -(a1 - b1) * (1.0 - exp(-365.0 / tau)) / (a1 * b2 - a2 * b1);

    // Compute body weight.
    double BW = F + L + G + W + ECF;

    // Compute energy expenditure.
    double EE =
        (shift + K + gamma_F * F + gamma_L * L + delta * BW + TEF + AT + EI * x) / (1.0 + x);

    // New simulated person state.
    return SimulatePersonState{.H = H,
                               .BW = BW,
                               .PAL = PAL,
                               .RMR = RMR,
                               .F = F,
                               .L = L,
                               .ECF = ECF,
                               .G = G,
                               .W = W,
                               .EE = EE,
                               .EI = EI,
                               .adjust = adjust};
}

std::unordered_map<core::Identifier, double>
EnergyBalanceModel::compute_nutrient_intakes(const Person &person) const {
    std::unordered_map<core::Identifier, double> nutrient_intakes;
    for (const auto &[food_key, nutrient_coefficients] : nutrient_equations_) {
        double food_intake = person.get_risk_factor_value(food_key);
        for (const auto &[nutrient_key, nutrient_coefficient] : nutrient_coefficients) {
            double delta_nutrient = food_intake * nutrient_coefficient;
            nutrient_intakes[nutrient_key] += delta_nutrient;
        }
    }
    return nutrient_intakes;
}

double EnergyBalanceModel::compute_EI(
    const std::unordered_map<core::Identifier, double> &nutrient_intakes) const {
    double EI = 0.0;
    for (const auto &[nutrient_key, energy_coefficient] : energy_equation_) {
        double delta_energy = nutrient_intakes.at(nutrient_key) * energy_coefficient;
        EI += delta_energy;
    }
    return EI;
}

double EnergyBalanceModel::compute_G(double CI, double CI_0, double G_0) const {
    double k_G = CI_0 / (G_0 * G_0);
    return sqrt(CI / k_G);
}

double EnergyBalanceModel::compute_ECF(double EI, double EI_0, double CI, double CI_0,
                                       double ECF_0) const {
    double Na_b = 4000.0;
    double Na_f = Na_b * EI / EI_0;
    double Delta_Na_diet = Na_f - Na_b;
    return ECF_0 + (Delta_Na_diet - xi_CI * (1.0 - CI / CI_0)) / xi_Na;
}

double EnergyBalanceModel::compute_RMR(double BW, double H, unsigned int age,
                                       core::Gender gender) const {
    double RMR = 9.99 * BW + 6.25 * H * 100.0 - 4.92 * age;
    RMR += gender == core::Gender::male ? 5.0 : -161.0;
    return RMR * 4.184; // kcal to kJ
}

double EnergyBalanceModel::bounded_nutrient_value(const core::Identifier &nutrient,
                                                  double value) const {
    const auto &range = nutrient_ranges_.at(nutrient);
    return std::clamp(range.first, range.second, value);
}

EnergyBalanceModelDefinition::EnergyBalanceModelDefinition(
    std::unordered_map<core::Identifier, double> energy_equation,
    std::unordered_map<core::Identifier, std::pair<double, double>> nutrient_ranges,
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
    std::unordered_map<core::Identifier, std::optional<double>> food_prices,
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
