#include "HealthGPS.Core/exception.h"

#include "kevin_hall_model.h"
#include "runtime_context.h"
#include "sync_message.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace { // anonymous namespace

using KevinHallAdjustmentMessage =
    hgps::SyncDataMessage<hgps::UnorderedMap2d<hgps::core::Gender, int, double>>;

} // anonymous namespace

/*
 * Suppress this clang-tidy warning for now, because most (all?) of these methods won't
 * be suitable for converting to statics once the model is finished.
 */
// NOLINTBEGIN(readability-convert-member-functions-to-static)
namespace hgps {

KevinHallModel::KevinHallModel(
    const RiskFactorSexAgeTable &expected,
    const std::unordered_map<core::Identifier, double> &energy_equation,
    const std::unordered_map<core::Identifier, core::DoubleInterval> &nutrient_ranges,
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations,
    const std::unordered_map<core::Identifier, std::optional<double>> &food_prices,
    const std::unordered_map<core::Gender, std::vector<double>> &weight_quantiles,
    const std::vector<double> &epa_quantiles,
    const std::unordered_map<core::Gender, double> &height_stddev, double height_slope)
    : RiskFactorAdjustableModel{expected}, energy_equation_{energy_equation},
      nutrient_ranges_{nutrient_ranges}, nutrient_equations_{nutrient_equations},
      food_prices_{food_prices}, weight_quantiles_{weight_quantiles}, epa_quantiles_{epa_quantiles},
      height_stddev_{height_stddev}, height_slope_{height_slope} {

    if (energy_equation_.empty()) {
        throw core::HgpsException("Energy equation mapping is empty");
    }
    if (nutrient_ranges_.empty()) {
        throw core::HgpsException("Nutrient range mapping is empty");
    }
    if (nutrient_equations_.empty()) {
        throw core::HgpsException("Nutrient equation mapping is empty");
    }
    if (food_prices_.empty()) {
        throw core::HgpsException("Food price mapping is empty");
    }
    if (weight_quantiles_.empty()) {
        throw core::HgpsException("Weight quantiles mapping is empty");
    }
    if (epa_quantiles_.empty()) {
        throw core::HgpsException("Energy Physical Activity quantiles mapping is empty");
    }
    if (height_stddev_.empty()) {
        throw core::HgpsException("Height standard deviation mapping is empty");
    }
}

RiskFactorModelType KevinHallModel::type() const noexcept { return RiskFactorModelType::Dynamic; }

std::string KevinHallModel::name() const noexcept { return "Dynamic"; }

void KevinHallModel::generate_risk_factors(RuntimeContext &context) {

    // Initialise everyone.
    for (auto &person : context.population()) {
        initialise_nutrient_intakes(person);
        initialise_energy_intake(person);
        initialise_weight(person);
    }

    // Adjust weight mean to match expected.
    adjust_risk_factors(context, {"Weight"_id});

    // Compute weight power means by sex and age.
    auto W_power_means = compute_mean_weight(context.population(), height_slope_);

    // Initialise everyone.
    for (auto &person : context.population()) {
        double W_power_mean = W_power_means.at(person.gender, person.age);
        initialise_height(person, W_power_mean, context.random());
        initialise_kevin_hall_state(person);
        compute_bmi(person);
    }
}

void KevinHallModel::update_risk_factors(RuntimeContext &context) {

    // Initialise newborns and update others.
    for (auto &person : context.population()) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            initialise_nutrient_intakes(person);
            initialise_energy_intake(person);
            initialise_weight(person);
        } else {
            update_nutrient_intakes(person);
            update_energy_intake(person);
        }
    }

    // TODO: This newborn adjustment needs sending to intervention scenario -- see #266.
    // Adjust newborn weight to match expected.
    const auto &expected = get_risk_factor_expected();
    auto W_newborn_means = compute_mean_weight(context.population(), std::nullopt, 0);
    for (auto &person : context.population()) {
        // Ignore if inactive or not a newborn.
        if (!person.is_active() || person.age != 0) {
            continue;
        }

        double W_expected = expected.at(person.gender, "Weight"_id).at(0);
        double W_mean = W_newborn_means.at(person.gender, 0);
        double adjustment = W_expected - W_mean;
        person.risk_factors.at("Weight"_id) -= adjustment;
    }

    // Compute newborn weight power means by sex.
    auto W_newborn_power_means = compute_mean_weight(context.population(), height_slope_, 0);

    // Initialise newborns.
    for (auto &person : context.population()) {
        // Ignore if inactive or not a newborn.
        if (!person.is_active() || person.age != 0) {
            continue;
        }

        double W_power_mean = W_newborn_power_means.at(person.gender, person.age);
        initialise_height(person, W_power_mean, context.random());
        initialise_kevin_hall_state(person);
    }

    // Update: different model for children and adults (no newborns).
    for (auto &person : context.population()) {
        // Ignore if inactive or newborn.
        if (!person.is_active() || person.age == 0) {
            continue;
        }

        if (person.age < kevin_hall_age_min) {
            initialise_weight(person);
        } else {
            kevin_hall_run(person);
        }
    }

    // Baseline scenatio: compute adjustments.
    KevinHallAdjustmentTable adjustments;
    if (context.scenario().type() == ScenarioType::baseline) {
        adjustments = compute_kevin_hall_adjustments(context.population());
    }

    // Intervention scenario: recieve adjustments from baseline scenario.
    else {
        auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
        if (!message.has_value()) {
            throw core::HgpsException(
                "Simulation out of sync, receive Kevin Hall adjustments message has timed out");
        }

        auto &basePtr = message.value();
        auto *messagePrt = dynamic_cast<KevinHallAdjustmentMessage *>(basePtr.get());
        if (!messagePrt) {
            throw core::HgpsException(
                "Simulation out of sync, failed to receive a Kevin Hall adjustments message");
        }

        adjustments = messagePrt->data();
    }

    // Adjust: different model for children and adults (no newborns).
    for (auto &person : context.population()) {
        // Ignore if inactive or newborn.
        if (!person.is_active() || person.age == 0) {
            continue;
        }

        auto &adjustment = adjustments.at(person.gender, person.age);
        if (person.age < kevin_hall_age_min) {
            initialise_kevin_hall_state(person, adjustment);
        } else {
            kevin_hall_adjust(person, adjustment);
        }
    }

    // Baseline scenario: send adjustments to intervention scenario.
    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<KevinHallAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(adjustments)));
    }

    // Compute weight power means by sex and age.
    auto W_power_means = compute_mean_weight(context.population(), height_slope_);

    // Update: (no newborns or 19 and over).
    for (auto &person : context.population()) {
        // Ignore if inactive or newborn or aged 19 and over.
        if (!person.is_active() || person.age == 0 || person.age >= 19) {
            continue;
        }

        double W_power_mean = W_power_means.at(person.gender, person.age);
        update_height(person, W_power_mean);
    }

    // Compute BMI values for everyone.
    for (auto &person : context.population()) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        compute_bmi(person);
    }
}

KevinHallAdjustmentTable
KevinHallModel::compute_kevin_hall_adjustments(Population &population) const {
    const auto &expected = get_risk_factor_expected();
    auto weight_means = compute_mean_weight(population);

    // Compute adjustments.
    auto adjustments = KevinHallAdjustmentTable{};
    for (const auto &[sex, weight_means_by_sex] : weight_means) {
        adjustments.emplace_row(sex, std::unordered_map<int, double>{});
        for (const auto &[age, weight_mean] : weight_means_by_sex) {
            double weight_expected = expected.at(sex, "Weight"_id).at(age);
            adjustments.at(sex)[age] = weight_expected - weight_mean;
        }
    }

    return adjustments;
}

void KevinHallModel::initialise_nutrient_intakes(Person &person) const {

    // Initialise nutrient intakes.
    compute_nutrient_intakes(person);

    // Start with previous = current.
    double carbohydrate = person.risk_factors.at("Carbohydrate"_id);
    person.risk_factors["Carbohydrate_previous"_id] = carbohydrate;
    double sodium = person.risk_factors.at("Sodium"_id);
    person.risk_factors["Sodium_previous"_id] = sodium;
}

void KevinHallModel::update_nutrient_intakes(Person &person) const {

    // Set previous nutrient intakes.
    double previous_carbohydrate = person.risk_factors.at("Carbohydrate"_id);
    person.risk_factors.at("Carbohydrate_previous"_id) = previous_carbohydrate;
    double previous_sodium = person.risk_factors.at("Sodium"_id);
    person.risk_factors.at("Sodium_previous"_id) = previous_sodium;

    // Update nutrient intakes.
    compute_nutrient_intakes(person);
}

void KevinHallModel::compute_nutrient_intakes(Person &person) const {

    // Reset nutrient intakes to zero.
    for (const auto &[nutrient_key, unused] : energy_equation_) {
        person.risk_factors[nutrient_key] = 0.0;
    }

    // Compute nutrient intakes from food intakes.
    for (const auto &[food_key, nutrient_coefficients] : nutrient_equations_) {
        double food_intake = person.risk_factors.at(food_key);
        for (const auto &[nutrient_key, nutrient_coefficient] : nutrient_coefficients) {
            person.risk_factors.at(nutrient_key) += food_intake * nutrient_coefficient;
        }
    }
}

void KevinHallModel::initialise_energy_intake(Person &person) const {

    // Initialise energy intake.
    compute_energy_intake(person);

    // Start with previous = current.
    double energy_intake = person.risk_factors.at("EnergyIntake"_id);
    person.risk_factors["EnergyIntake_previous"_id] = energy_intake;
}

void KevinHallModel::update_energy_intake(Person &person) const {

    // Set previous energy intake.
    double previous_energy_intake = person.risk_factors.at("EnergyIntake"_id);
    person.risk_factors.at("EnergyIntake_previous"_id) = previous_energy_intake;

    // Update energy intake.
    compute_energy_intake(person);
}

void KevinHallModel::compute_energy_intake(Person &person) const {

    // Reset energy intake to zero.
    const auto energy_intake_key = "EnergyIntake"_id;
    person.risk_factors[energy_intake_key] = 0.0;

    // Compute energy intake from nutrient intakes.
    for (const auto &[nutrient_key, energy_coefficient] : energy_equation_) {
        double nutrient_intake = person.risk_factors.at(nutrient_key);
        person.risk_factors.at(energy_intake_key) += nutrient_intake * energy_coefficient;
    }
}

void KevinHallModel::initialise_kevin_hall_state(Person &person,
                                                 std::optional<double> adjustment) const {

    // Apply optional weight adjustment.
    if (adjustment.has_value()) {
        person.risk_factors.at("Weight"_id) += adjustment.value();
    }

    // Get already computed values.
    double H = person.risk_factors.at("Height"_id);
    double BW = person.risk_factors.at("Weight"_id);
    double PAL = person.risk_factors.at("PhysicalActivity"_id);
    double EI = person.risk_factors.at("EnergyIntake"_id);

    // Body fat.
    double F;
    if (person.gender == core::Gender::female) {
        F = BW * (0.14 * person.age + 39.96 * log(BW / pow(H / 100, 2.0)) - 102.01) / 100.0;
    } else if (person.gender == core::Gender::male) {
        F = BW * (0.14 * person.age + 37.31 * log(BW / pow(H / 100, 2.0)) - 103.94) / 100.0;
    } else {
        throw core::HgpsException("Unknown gender");
    }

    if (F < 0.0) {
        // HACK: deal with negative body fat.
        F = 0.2 * BW;
    }

    // Glycogen, water and extracellular fluid.
    double G = 0.01 * BW;
    double W = 2.7 * G;
    double ECF = 0.7 * 0.235 * BW;

    // Lean tissue.
    double L = BW - F - G - W - ECF;

    // Model intercept value.
    double delta = compute_delta(person.age, person.gender, PAL, BW, H);
    double K = EI - (gamma_F * F + gamma_L * L + delta * BW);

    // Compute energy expenditure.
    double C = 10.4 * rho_L / rho_F;
    double p = C / (C + F);
    double x = p * eta_L / rho_L + (1.0 - p) * eta_F / rho_F;
    double EE = compute_EE(BW, F, L, EI, K, delta, x);

    // Set new state.
    person.risk_factors["BodyFat"_id] = F;
    person.risk_factors["LeanTissue"_id] = L;
    person.risk_factors["Glycogen"_id] = G;
    person.risk_factors["ExtracellularFluid"_id] = ECF;
    person.risk_factors["Intercept_K"_id] = K;

    // Set new energy expenditure (may not exist yet).
    person.risk_factors["EnergyExpenditure"_id] = EE;
}

void KevinHallModel::kevin_hall_run(Person &person) const {

    // Get initial body weight.
    double BW_0 = person.risk_factors.at("Weight"_id);

    // Compute energy cost per unit body weight.
    double PAL = person.risk_factors.at("PhysicalActivity"_id);
    double H = person.risk_factors.at("Height"_id);
    double delta = compute_delta(person.age, person.gender, PAL, BW_0, H);

    // Get carbohydrate intake and energy intake.
    double CI_0 = person.risk_factors.at("Carbohydrate_previous"_id);
    double CI = person.risk_factors.at("Carbohydrate"_id);
    double EI_0 = person.risk_factors.at("EnergyIntake_previous"_id);
    double EI = person.risk_factors.at("EnergyIntake"_id);
    double delta_EI = EI - EI_0;

    // Compute thermic effect of food.
    double TEF = beta_TEF * delta_EI;

    // Compute adaptive thermogenesis.
    double AT = beta_AT * delta_EI;

    // Compute glycogen and water.
    double G_0 = person.risk_factors.at("Glycogen"_id);
    double G = compute_G(CI, CI_0, G_0);
    double W = 2.7 * G;

    // Compute extracellular fluid.
    double Na_0 = person.risk_factors.at("Sodium_previous"_id);
    double Na = person.risk_factors.at("Sodium"_id);
    double delta_Na = Na - Na_0;
    double ECF_0 = person.risk_factors.at("ExtracellularFluid"_id);
    double ECF = compute_ECF(delta_Na, CI, CI_0, ECF_0);

    // Get initial body fat and lean tissue.
    double F_0 = person.risk_factors.at("BodyFat"_id);
    double L_0 = person.risk_factors.at("LeanTissue"_id);

    // Get intercept value.
    double K = person.risk_factors.at("Intercept_K"_id);

    // Energy partitioning.
    double C = 10.4 * rho_L / rho_F;
    double p = C / (C + F_0);
    double x = p * eta_L / rho_L + (1.0 - p) * eta_F / rho_F;

    // First equation ax + by = e.
    double a1 = p * rho_F;
    double b1 = -(1.0 - p) * rho_L;
    double c1 = p * rho_F * F_0 - (1 - p) * rho_L * L_0;

    // Second equation cx + dy = f.
    double a2 = gamma_F + delta;
    double b2 = gamma_L + delta;
    double c2 = EI - K - TEF - AT - delta * (G + W + ECF);

    // Compute body fat and lean tissue steady state.
    double steady_F = -(b1 * c2 - b2 * c1) / (a1 * b2 - a2 * b1);
    double steady_L = -(c1 * a2 - c2 * a1) / (a1 * b2 - a2 * b1);

    // Compute time constant.
    double tau = rho_L * rho_F * (1.0 + x) /
                 ((gamma_F + delta) * (1.0 - p) * rho_L + (gamma_L + delta) * p * rho_F);

    // Compute body fat and lean tissue.
    double F = steady_F - (steady_F - F_0) * exp(-365.0 / tau);
    double L = steady_L - (steady_L - L_0) * exp(-365.0 / tau);

    // Compute body weight.
    double BW = F + L + G + W + ECF;

    // Set new state.
    person.risk_factors.at("Glycogen"_id) = G;
    person.risk_factors.at("ExtracellularFluid"_id) = ECF;
    person.risk_factors.at("BodyFat"_id) = F;
    person.risk_factors.at("LeanTissue"_id) = L;
    person.risk_factors.at("Weight"_id) = BW;
}

double KevinHallModel::compute_G(double CI, double CI_0, double G_0) const {
    double k_G = CI_0 / (G_0 * G_0);
    return sqrt(CI / k_G);
}

double KevinHallModel::compute_ECF(double delta_Na, double CI, double CI_0, double ECF_0) const {
    return ECF_0 + (delta_Na - xi_CI * (1.0 - CI / CI_0)) / xi_Na;
}

double KevinHallModel::compute_delta(int age, core::Gender sex, double PAL, double BW,
                                     double H) const {
    // Resting metabolic rate (Mifflin-St Jeor).
    double RMR = 9.99 * BW + 6.25 * H - 4.92 * age;
    RMR += sex == core::Gender::male ? 5.0 : -161.0;
    RMR *= 4.184; // From kcal to kJ

    // Energy expenditure per kg of body weight.
    return ((1.0 - beta_TEF) * PAL - 1.0) * RMR / BW;
}

double KevinHallModel::compute_EE(double BW, double F, double L, double EI, double K, double delta,
                                  double x) const {
    // OLD: return (K + gamma_F * F + gamma_L * L + delta * BW + TEF + AT + EI * x) / (1.0 + x);
    // TODO: Double-check new calculation below is correct.
    return (K + gamma_F * F + gamma_L * L + delta * BW + beta_TEF + beta_AT +
            x * (EI - rho_G * 0.0)) /
           (1 + x);
}

void KevinHallModel::compute_bmi(Person &person) const {
    auto w = person.risk_factors.at("Weight"_id);
    auto h = person.risk_factors.at("Height"_id) / 100;
    person.risk_factors["BMI"_id] = w / (h * h);
}

void KevinHallModel::initialise_weight(Person &person) const {
    const auto &expected = get_risk_factor_expected();

    // Compute E/PA expected.
    double ei_expected = expected.at(person.gender, "EnergyIntake"_id).at(person.age);
    double pa_expected = expected.at(person.gender, "PhysicalActivity"_id).at(person.age);
    double epa_expected = ei_expected / pa_expected;

    // Compute E/PA actual.
    double ei_actual = person.risk_factors.at("EnergyIntake"_id);
    double pa_actual = person.risk_factors.at("PhysicalActivity"_id);
    double epa_actual = ei_actual / pa_actual;

    // Compute E/PA quantile.
    double epa_quantile = epa_actual / epa_expected;

    // Compute new weight.
    double w_expected = expected.at(person.gender, "Weight"_id).at(person.age);
    double w_quantile = get_weight_quantile(epa_quantile, person.gender);
    person.risk_factors["Weight"_id] = w_expected * w_quantile;
}

void KevinHallModel::kevin_hall_adjust(Person &person, double adjustment) const {
    double H = person.risk_factors.at("Height"_id);
    double BW_0 = person.risk_factors.at("Weight"_id);
    double F_0 = person.risk_factors.at("BodyFat"_id);
    double L_0 = person.risk_factors.at("LeanTissue"_id);
    double G = person.risk_factors.at("Glycogen"_id);
    double W = 2.7 * G;
    double ECF_0 = person.risk_factors.at("ExtracellularFluid"_id);
    double PAL = person.risk_factors.at("PhysicalActivity"_id);
    double EI = person.risk_factors.at("EnergyIntake"_id);
    double K_0 = person.risk_factors.at("Intercept_K"_id);

    double C = 10.4 * rho_L / rho_F;

    // Compute previous energy expenditure.
    double p_0 = C / (C + F_0);
    double x_0 = p_0 * eta_L / rho_L + (1.0 - p_0) * eta_F / rho_F;
    double delta_0 = compute_delta(person.age, person.gender, PAL, BW_0, H);
    double EE_0 = compute_EE(BW_0, F_0, L_0, EI, K_0, delta_0, x_0);

    // Adjust weight and compute adjustment ratio for other factors.
    double BW = BW_0 + adjustment;
    double ratio = (BW - G - W) / (BW_0 - G - W);

    // Adjust other factors to compensate.
    double F = F_0 * ratio;
    double L = L_0 * ratio;
    double ECF = ECF_0 * ratio;

    // Compute new energy expenditure.
    double p = C / (C + F);
    double x = p * eta_L / rho_L + (1.0 - p) * eta_F / rho_F;
    double delta = compute_delta(person.age, person.gender, PAL, BW, H);
    double EE = compute_EE(BW, F, L, EI, K_0, delta, x);

    // Adjust model intercept.
    double K = K_0 + (1 + x) * (EE_0 - EE);

    // Set new state.
    person.risk_factors.at("Weight"_id) = BW;
    person.risk_factors.at("BodyFat"_id) = F;
    person.risk_factors.at("LeanTissue"_id) = L;
    person.risk_factors.at("ExtracellularFluid"_id) = ECF;
    person.risk_factors.at("Intercept_K"_id) = K;

    // Set new energy expenditure (may not exist yet).
    person.risk_factors["EnergyExpenditure"_id] = EE;
}

double KevinHallModel::get_weight_quantile(double epa_quantile, core::Gender sex) const {

    // Compute Energy Physical Activity percentile (taking midpoint of duplicates).
    auto epa_range = std::equal_range(epa_quantiles_.begin(), epa_quantiles_.end(), epa_quantile);
    auto epa_index = static_cast<double>(std::distance(epa_quantiles_.begin(), epa_range.first));
    epa_index += std::distance(epa_range.first, epa_range.second) / 2.0;
    auto epa_percentile = epa_index / epa_quantiles_.size();

    // Find weight quantile.
    auto weight_index = static_cast<size_t>(epa_percentile * (weight_quantiles_.size() - 1));
    return weight_quantiles_.at(sex)[weight_index];
}

KevinHallAdjustmentTable KevinHallModel::compute_mean_weight(Population &population,
                                                             std::optional<double> power,
                                                             std::optional<unsigned> age) const {

    // Local struct to hold count and sum of weight powers.
    struct SumCount {
      public:
        void append(double value) noexcept {
            sum_ += value;
            count_++;
        }

        double mean() const noexcept { return sum_ / count_; }

      private:
        double sum_{};
        int count_{};
    };

    // Compute sums and counts of weight powers for sex and age.
    auto sumcounts = UnorderedMap2d<core::Gender, int, SumCount>{};
    sumcounts.emplace_row(core::Gender::female, std::unordered_map<int, SumCount>{});
    sumcounts.emplace_row(core::Gender::male, std::unordered_map<int, SumCount>{});
    for (const auto &person : population) {
        bool age_mismatch = age.has_value() && person.age != age.value();
        if (!person.is_active() || age_mismatch) {
            continue;
        }

        double value;
        if (power.has_value()) {
            value = pow(person.risk_factors.at("Weight"_id), power.value());
        } else {
            value = person.risk_factors.at("Weight"_id);
        }

        sumcounts.at(person.gender)[person.age].append(value);
    }

    // Compute means of weight powers for sex and age.
    auto means = KevinHallAdjustmentTable{};
    for (const auto &[sex, sumcounts_by_sex] : sumcounts) {
        means.emplace_row(sex, std::unordered_map<int, double>{});
        for (const auto &[age, sumcount] : sumcounts_by_sex) {
            means.at(sex)[age] = sumcount.mean();
        }
    }

    return means;
}

void KevinHallModel::initialise_height(Person &person, double W_power_mean, Random &random) const {

    // Initialise lifelong height residual.
    double H_residual = random.next_normal(0, height_stddev_.at(person.gender));
    person.risk_factors["Height_residual"_id] = H_residual;

    // Initialise height.
    update_height(person, W_power_mean);
}

void KevinHallModel::update_height(Person &person, double W_power_mean) const {
    double W = person.risk_factors.at("Weight"_id);
    double H_expected = get_risk_factor_expected().at(person.gender, "Height"_id).at(person.age);
    double H_residual = person.risk_factors.at("Height_residual"_id);
    double stddev = height_stddev_.at(person.gender);
    double slope = height_slope_;

    // Compute height.
    double exp_norm_mean = exp(0.5 * stddev * stddev);
    double H = H_expected * (pow(W, slope) / W_power_mean) * (exp(H_residual) / exp_norm_mean);

    // Set height (may not exist yet).
    person.risk_factors["Height"_id] = H;
}

KevinHallModelDefinition::KevinHallModelDefinition(
    RiskFactorSexAgeTable expected, std::unordered_map<core::Identifier, double> energy_equation,
    std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges,
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
    std::unordered_map<core::Identifier, std::optional<double>> food_prices,
    std::unordered_map<core::Gender, std::vector<double>> weight_quantiles,
    std::vector<double> epa_quantiles, std::unordered_map<core::Gender, double> height_stddev,
    double height_slope)
    : RiskFactorAdjustableModelDefinition{std::move(expected)},
      energy_equation_{std::move(energy_equation)}, nutrient_ranges_{std::move(nutrient_ranges)},
      nutrient_equations_{std::move(nutrient_equations)}, food_prices_{std::move(food_prices)},
      weight_quantiles_{std::move(weight_quantiles)}, epa_quantiles_{std::move(epa_quantiles)},
      height_stddev_{std::move(height_stddev)}, height_slope_{height_slope} {

    if (energy_equation_.empty()) {
        throw core::HgpsException("Energy equation mapping is empty");
    }
    if (nutrient_ranges_.empty()) {
        throw core::HgpsException("Nutrient ranges mapping is empty");
    }
    if (nutrient_equations_.empty()) {
        throw core::HgpsException("Nutrient equation mapping is empty");
    }
    if (food_prices_.empty()) {
        throw core::HgpsException("Food prices mapping is empty");
    }
    if (weight_quantiles_.empty()) {
        throw core::HgpsException("Weight quantiles mapping is empty");
    }
    if (epa_quantiles_.empty()) {
        throw core::HgpsException("Energy Physical Activity quantiles mapping is empty");
    }
    if (height_stddev_.empty()) {
        throw core::HgpsException("Height standard deviation mapping is empty");
    }
}

std::unique_ptr<RiskFactorModel> KevinHallModelDefinition::create_model() const {
    const auto &expected = get_risk_factor_expected();
    return std::make_unique<KevinHallModel>(expected, energy_equation_, nutrient_ranges_,
                                            nutrient_equations_, food_prices_, weight_quantiles_,
                                            epa_quantiles_, height_stddev_, height_slope_);
}

} // namespace hgps
// NOLINTEND(readability-convert-member-functions-to-static)
