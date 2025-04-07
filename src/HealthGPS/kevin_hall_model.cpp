#include "HealthGPS.Core/exception.h"
#include <fstream> // Add for file output
#include <iostream>

// Disable specific warnings from external dependencies
#ifdef _MSC_VER
#pragma warning(push)
// Eigen warnings
#pragma warning(disable : 26495) // Variable 'xxx' is uninitialized (from Eigen SIMD optimizations)

// fmt library warnings
#pragma warning(disable : 26498) // Variable 'xxx' is constexpr, mark variable constexpr (from fmt)

// lexer warnings
#pragma warning(disable : 26819) // Unannotated fallthrough between switch labels (from lexer)

// Low-level bitwise operations
#pragma warning(                                                                                   \
    disable : 6285) // Non-zero constant in bitwise-and operation (from core bit manipulation)
#endif

#include "demographic.h"
#include "kevin_hall_model.h"
#include "runtime_context.h"
#include "sync_message.h"

#include <algorithm>
#include <iterator>
#include <random>
#include <utility>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace { // anonymous namespace

using KevinHallAdjustmentMessage =
    hgps::SyncDataMessage<hgps::UnorderedMap2d<hgps::core::Gender, int, double>>;

// Helper function to create a debug CSV for invalid BMI calculations
void log_invalid_bmi_to_csv(const hgps::Person &person, double w, double h, double bmi,
                            const std::string &reason, bool create_header = false) {
    static std::ofstream debug_file;
    static bool file_initialized = false;

    // Initialize file if needed
    if (!file_initialized) {
        debug_file.open("invalid_bmi_debug.csv");
        file_initialized = true;

        // Write header
        debug_file << "PersonID,Age,Gender,BMI,Weight,Height,BodyFat,LeanTissue,Glycogen,ECF,";
        debug_file << "EnergyIntake,EnergyIntake_previous,PhysicalActivity,";
        debug_file << "Carbohydrate,Carbohydrate_previous,Sodium,Sodium_previous,";
        debug_file << "Intercept_K,Height_residual,Reason" << std::endl;
    }

    // Record gender as string
    std::string gender_str = (person.gender == hgps::core::Gender::male) ? "M" : "F";

    // Start with basic person info
    debug_file << person.id() << "," << person.age << "," << gender_str << "," << bmi << "," << w
               << "," << h << ",";

    // Add reason for invalid BMI
    debug_file << reason << std::endl;

    // Flush to ensure data is written immediately
    debug_file.flush();
}

} // anonymous namespace

/*
 * Suppress this clang-tidy warning for now, because most (all?) of these methods won't
 * be suitable for converting to statics once the model is finished.
 */
// NOLINTBEGIN(readability-convert-member-functions-to-static)
namespace hgps {

KevinHallModel::KevinHallModel(
    std::shared_ptr<RiskFactorSexAgeTable> expected,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    const std::unordered_map<core::Identifier, double> &energy_equation,
    const std::unordered_map<core::Identifier, core::DoubleInterval> &nutrient_ranges,
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations,
    const std::unordered_map<core::Identifier, std::optional<double>> &food_prices,
    const std::unordered_map<core::Gender, std::vector<double>> &weight_quantiles,
    const std::vector<double> &epa_quantiles,
    const std::unordered_map<core::Gender, double> &height_stddev,
    const std::unordered_map<core::Gender, double> &height_slope,
    std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models,
    std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> ethnicity_models,
    std::unordered_map<core::Income, LinearModelParams> income_models,
    double income_continuous_stddev)
    : RiskFactorAdjustableModel{std::move(expected), std::move(expected_trend),
                                std::move(trend_steps)},
      energy_equation_{energy_equation}, nutrient_ranges_{nutrient_ranges},
      nutrient_equations_{nutrient_equations}, food_prices_{food_prices},
      weight_quantiles_{weight_quantiles}, epa_quantiles_{epa_quantiles},
      height_stddev_{height_stddev}, height_slope_{height_slope},
      region_models_{std::move(region_models)}, ethnicity_models_{std::move(ethnicity_models)},
      income_models_{std::move(income_models)},
      income_continuous_stddev_{income_continuous_stddev} {}

RiskFactorModelType KevinHallModel::type() const noexcept { return RiskFactorModelType::Dynamic; }

std::string KevinHallModel::name() const noexcept { return "Dynamic"; }

// Modified: Mahima 25/02/2025
// Ensuring correct initialization order for population characteristics
void KevinHallModel::generate_risk_factors(RuntimeContext &context) {
    context_ = &context; // Store reference to the context

    // Step 1: Age and gender are already initialized by the population generator (demographic.cpp)
    // also in person.cpp, the person class maintains a deep copy of it

    // Step 2: Initialize fixed characteristics (region and ethnicity)
    // These are now initialized in demographic.cpp

    // Step 3: Initialize continuous income
    // This is now initialized in demographic.cpp

    // Step 4: Initialize income category
    // This is now initialized in demographic.cpp

    // Step 5: Initialize physical activity
    // This is now initialized in demographic.cpp

    // Step 6: Initialize remaining risk factors
    for (auto &person : context.population()) {
        initialise_nutrient_intakes(person, context.random());
        initialise_energy_intake(person);
        initialise_weight(context, person);
    }

    // Adjust weight mean to match expected
    adjust_risk_factors(context, {"Weight"_id}, std::nullopt, true);

    // Added for FINCH- Mahima
    //  Adjust physical activity mean to match expected
    adjust_risk_factors(context, {"PhysicalActivity"_id}, std::nullopt, false);

    // Compute weight power means by sex and age
    auto W_power_means = compute_mean_weight(context.population(), height_slope_);

    // Step 7: Initialize height (depends on weight)
    // Step 8: Calculate BMI (depends on weight and height)
    for (auto &person : context.population()) {
        double W_power_mean = W_power_means.at(person.gender, person.age);
        initialise_height(context, person, W_power_mean, context.random());
        initialise_kevin_hall_state(person);
        compute_bmi(person);
    }
}

void KevinHallModel::update_risk_factors(RuntimeContext &context) {
    context_ = &context; // Store reference to the context

    // Update income categories every 5 years
    update_income_category(context);

    // Update (initialise) newborns.
    update_newborns(context);

    // Update non-newborns.
    update_non_newborns(context);

    // Compute BMI values for everyone.
    for (auto &person : context.population()) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        compute_bmi(person);
        if (person.age == 18) {
            update_region(context, person, context.random());
        }
    }

    // Adjust physical activity mean to match expected
    adjust_risk_factors(context, {"PhysicalActivity"_id}, std::nullopt, false);
}

void KevinHallModel::update_newborns(RuntimeContext &context) const {

    // Initialise nutrient and energy intake and weight for newborns.
    for (auto &person : context.population()) {
        // Ignore if inactive or not newborn.
        if (!person.is_active() || (person.age != 0)) {
            continue;
        }

        initialise_nutrient_intakes(person, context.random());
        initialise_energy_intake(person);
        initialise_weight(context, person);
    }

    // TODO: This newborn adjustment needs sending to intervention scenario -- see #266.

    // NOTE: FOR REFACTORING: This block should eventually be replaced by a call to
    // `adjust_risk_factors(context, {"Weight"_id}, IntegerInterval{0, 0});` once age_range
    // is implemented in the adjustment method, as it is redundant and does not communicate
    // the newborn weight adjustment to the intervention (see #266).

    // NOTE: FOR REFACTORING: Then, this whole method can be replaced with a generalised
    // initialise method, which accepts an age range, and both the `generate_risk_factors`
    // (on `context.age_range()`) and `update_risk_factors` (on `IntegerInterval{0, 0}`)
    // can call this new method.

    // Adjust newborn weight to match expected.
    auto adjustments = compute_weight_adjustments(context, 0);
    for (auto &person : context.population()) {
        // Ignore if inactive or not newborn.
        if (!person.is_active() || (person.age != 0)) {
            continue;
        }

        double adjustment = adjustments.at(person.gender, person.age);
        person.risk_factors.at("Weight"_id) += adjustment;
    }

    // NOTE: FOR REFACTORING: End of semi-redundant block.

    // Adjust physical activity for newborns to match expected
    for (auto &person : context.population()) {
        // Ignore if inactive or not newborn.
        if (!person.is_active() || (person.age != 0)) {
            continue;
        }

        // Initialize physical activity if not already done
        if (!person.risk_factors.contains("PhysicalActivity"_id)) {
            initialise_physical_activity(context, person, context.random());
        }
    }

    // Adjust physical activity mean to match expected for newborns
    adjust_risk_factors(context, {"PhysicalActivity"_id}, std::nullopt, false);

    // Compute newborn weight power means by sex.
    auto W_power_means = compute_mean_weight(context.population(), height_slope_, 0);

    // Initialise height and other Kevin Hall state for newborns.
    for (auto &person : context.population()) {
        // Ignore if inactive or not newborn.
        if (!person.is_active() || (person.age != 0)) {
            continue;
        }

        double W_power_mean = W_power_means.at(person.gender, person.age);
        initialise_height(context, person, W_power_mean, context.random());
        initialise_kevin_hall_state(person);
    }
}

void KevinHallModel::update_non_newborns(RuntimeContext &context) const {

    // Update nutrient and energy intake for non-newborns.
    for (auto &person : context.population()) {
        // Ignore if inactive or newborn.
        if (!person.is_active() || (person.age == 0)) {
            continue;
        }

        update_nutrient_intakes(person);
        update_energy_intake(person);
    }

    // Update weight for non-newborns.
    for (auto &person : context.population()) {
        // Ignore if inactive or newborn.
        if (!person.is_active() || (person.age == 0)) {
            continue;
        }

        if (person.age < kevin_hall_age_min) {
            initialise_weight(context, person);
        } else {
            kevin_hall_run(person);
        }
    }

    // Compute (baseline) or receive (intervention) weight adjustments from baseline scenario.
    auto adjustments = receive_weight_adjustments(context);

    // Adjust weight and other Kevin Hall state for non-newborns.
    for (auto &person : context.population()) {
        // Ignore if inactive or newborn.
        if (!person.is_active() || (person.age == 0)) {
            continue;
        }

        double adjustment;
        if (adjustments.contains(person.gender, person.age)) {
            adjustment = adjustments.at(person.gender, person.age);
        } else {
            adjustment = 0.0;
        }

        if (person.age < kevin_hall_age_min) {
            initialise_kevin_hall_state(person, adjustment);
        } else {
            adjust_weight(person, adjustment);
        }
    }

    // Send (baseline) weight adjustments to intervention scenario.
    send_weight_adjustments(context, std::move(adjustments));

    // Compute weight power means by sex and age.
    auto W_power_means = compute_mean_weight(context.population(), height_slope_);

    // Update: (no newborns or at least the Kevin Hall minimum age).
    for (auto &person : context.population()) {
        // Ignore if inactive or newborn or at least the Kevin Hall minimum age.
        if (!person.is_active() || ((person.age == 0) || (person.age >= kevin_hall_age_min))) {
            continue;
        }

        double W_power_mean = W_power_means.at(person.gender, person.age);
        update_height(context, person, W_power_mean);
    }
}

KevinHallAdjustmentTable KevinHallModel::receive_weight_adjustments(RuntimeContext &context) const {
    KevinHallAdjustmentTable adjustments;

    // Baseline scenatio: compute adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        return compute_weight_adjustments(context);
    }

    // Intervention scenario: receive adjustments from baseline scenario.
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

    return messagePrt->data();
}

void KevinHallModel::send_weight_adjustments(RuntimeContext &context,
                                             KevinHallAdjustmentTable &&adjustments) const {

    // Baseline scenario: send adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<KevinHallAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(adjustments)));
    }
}

KevinHallAdjustmentTable
KevinHallModel::compute_weight_adjustments(RuntimeContext &context,
                                           std::optional<unsigned> age) const {
    auto W_means = compute_mean_weight(context.population(), std::nullopt, age);

    // Compute adjustments.
    auto adjustments = KevinHallAdjustmentTable{};
    for (const auto &[W_mean_sex, W_means_by_sex] : W_means) {
        adjustments.emplace_row(W_mean_sex, std::unordered_map<int, double>{});
        for (const auto &[W_mean_age, W_mean] : W_means_by_sex) {
            // NOTE: we only have the ages we requested here.
            double W_expected =
                get_expected(context, W_mean_sex, W_mean_age, "Weight"_id, std::nullopt, true);
            adjustments.at(W_mean_sex)[W_mean_age] = W_expected - W_mean;
        }
    }

    return adjustments;
}

double KevinHallModel::get_expected(RuntimeContext &context, core::Gender sex, int age,
                                    const core::Identifier &factor, OptionalRange range,
                                    bool apply_trend) const noexcept {

    // Compute expected nutrient intakes from expected food intakes.
    if (energy_equation_.contains(factor)) {
        double nutrient_intake = 0.0;
        for (const auto &[food_key, nutrient_coefficients] : nutrient_equations_) {
            double food_intake =
                get_expected(context, sex, age, food_key, std::nullopt, apply_trend);
            if (nutrient_coefficients.contains(factor)) {
                nutrient_intake += food_intake * nutrient_coefficients.at(factor);
            }
        }
        return nutrient_intake;
    }

    // Compute expected energy intake from expected nutrient intakes.
    if (factor == "EnergyIntake"_id) {

        // Non-trend case.
        if (!apply_trend) {
            return RiskFactorAdjustableModel::get_expected(context, sex, age, "EnergyIntake"_id,
                                                           std::nullopt, false);
        }

        // Trend case.
        double energy_intake = 0.0;
        for (const auto &[nutrient_key, energy_coefficient] : energy_equation_) {
            double nutrient_intake =
                get_expected(context, sex, age, nutrient_key, std::nullopt, true);
            energy_intake += nutrient_intake * energy_coefficient;
        }
        return energy_intake;
    }

    // Compute expected body weight.
    if (factor == "Weight"_id) {

        // Non-trend case.
        if (!apply_trend) {
            return RiskFactorAdjustableModel::get_expected(context, sex, age, "Weight"_id,
                                                           std::nullopt, false);
        }

        // Child case.
        if (age < kevin_hall_age_min) {
            double weight = RiskFactorAdjustableModel::get_expected(context, sex, age, "Weight"_id,
                                                                    std::nullopt, false);
            double energy_without_trend =
                get_expected(context, sex, age, "EnergyIntake"_id, std::nullopt, false);
            double energy_with_trend =
                get_expected(context, sex, age, "EnergyIntake"_id, std::nullopt, true);
            weight *= pow(energy_with_trend / energy_without_trend, 0.45);
            return weight;
        }

        // Adult case.
        double weight = 16.1161;
        weight += 0.06256 * get_expected(context, sex, age, "EnergyIntake"_id, std::nullopt, true);
        weight -= 0.6256 * get_expected(context, sex, age, "Height"_id, std::nullopt, false);
        weight += 0.4925 * age;
        weight -= 16.6166 * Person::gender_to_value(sex);
        return weight;
    }

    // Fall back to base class implementation for other factors.
    return RiskFactorAdjustableModel::get_expected(context, sex, age, factor, range, apply_trend);
}

void KevinHallModel::initialise_nutrient_intakes(Person &person,
                                                 [[maybe_unused]] Random &random) const {
    // Initialise base nutrient intakes
    compute_nutrient_intakes(person);

    // Get current values
    double carbohydrate = person.risk_factors.at("Carbohydrate"_id);
    double sodium = person.risk_factors.at("Sodium"_id);

    // Store values with random variations
    person.risk_factors.at("Carbohydrate"_id) = carbohydrate;
    person.risk_factors["Carbohydrate_previous"_id] = carbohydrate;
    person.risk_factors.at("Sodium"_id) = sodium;
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

    // Log intermediate values to detect division by zero or other issues
    if (std::abs(a1 * b2 - a2 * b1) < 1e-10) {
        // We're about to divide by a near-zero value, which will cause instability
        std::ofstream debug_file("kevin_hall_division_error.csv", std::ios_base::app);
        debug_file << person.id() << "," << person.age << ","
                   << (person.gender == core::Gender::male ? "M" : "F") << ","
                   << "Near-zero denominator,"
                   << "a1=" << a1 << ",b2=" << b2 << ",a2=" << a2 << ",b1=" << b1 << ","
                   << "denom=" << (a1 * b2 - a2 * b1) << ","
                   << "F_0=" << F_0 << ",L_0=" << L_0 << ","
                   << "BW_0=" << BW_0 << ",delta=" << delta << std::endl;

        // Set weight to expected value and return early to avoid division by zero
        double expected_weight =
            get_expected(*context_, person.gender, person.age, "Weight"_id, std::nullopt, true);
        person.risk_factors.at("Weight"_id) = expected_weight;
        return;
    }

    // Compute body fat and lean tissue steady state.
    double steady_F = -(b1 * c2 - b2 * c1) / (a1 * b2 - a2 * b1);
    double steady_L = -(c1 * a2 - c2 * a1) / (a1 * b2 - a2 * b1);

    // Check for unreasonable steady state values
    if (steady_F < 0 || steady_F > 150 || steady_L < 0 || steady_L > 200) {
        // Log extreme steady state values
        std::ofstream debug_file("kevin_hall_extreme_steady_state.csv", std::ios_base::app);
        debug_file << person.id() << "," << person.age << ","
                   << (person.gender == core::Gender::male ? "M" : "F") << ","
                   << "Extreme steady state,"
                   << "steady_F=" << steady_F << ",steady_L=" << steady_L << ","
                   << "a1=" << a1 << ",b1=" << b1 << ",c1=" << c1 << ","
                   << "a2=" << a2 << ",b2=" << b2 << ",c2=" << c2 << ","
                   << "F_0=" << F_0 << ",L_0=" << L_0 << std::endl;
    }

    // Compute time constant.
    double tau = rho_L * rho_F * (1.0 + x) /
                 ((gamma_F + delta) * (1.0 - p) * rho_L + (gamma_L + delta) * p * rho_F);

    // Check for unreasonable time constant
    if (tau <= 0 || tau > 10000) {
        // Log extreme time constant values
        std::ofstream debug_file("kevin_hall_extreme_tau.csv", std::ios_base::app);
        debug_file << person.id() << "," << person.age << ","
                   << (person.gender == core::Gender::male ? "M" : "F") << ","
                   << "Extreme tau value,"
                   << "tau=" << tau << ",p=" << p << ",x=" << x << ","
                   << "delta=" << delta << std::endl;

        // Clamp tau to reasonable range
        tau = std::max(1.0, std::min(2000.0, tau));
    }

    // Compute body fat and lean tissue.
    double F = steady_F - (steady_F - F_0) * exp(-365.0 / tau);
    double L = steady_L - (steady_L - L_0) * exp(-365.0 / tau);

    // Compute body weight.
    double BW = F + L + G + W + ECF;

    // Get weight range from config
    double MIN_WEIGHT = 1.0;   // Default minimum weight (kg)
    double MAX_WEIGHT = 210.0; // Default maximum weight (kg)

    // Get actual ranges from config if available
    if (nutrient_ranges_.contains("Weight"_id)) {
        const auto &weight_range = nutrient_ranges_.at("Weight"_id);
        MIN_WEIGHT = weight_range.lower();
        MAX_WEIGHT = weight_range.upper();
    }

    // Check for unreasonable or invalid weight calculation
    if (std::isnan(BW) || std::isinf(BW) || BW < MIN_WEIGHT || BW > MAX_WEIGHT) {
        // Create a detailed diagnostic log
        std::ofstream debug_file("kevin_hall_invalid_weight.csv", std::ios_base::app);

        // Add header if file is newly created
        if (debug_file.tellp() == 0) {
            debug_file << "PersonID,Age,Gender,Reason,Initial_Weight,Calculated_Weight,"
                       << "F,L,G,W,ECF,PAL,EI,CI,Na,delta,tau,"
                       << "steady_F,steady_L,F_0,L_0" << std::endl;
        }

        // Prepare reason string
        std::string reason;
        if (std::isnan(BW))
            reason = "NaN weight";
        else if (std::isinf(BW))
            reason = "Infinite weight";
        else if (BW < MIN_WEIGHT)
            reason = "Too low weight";
        else if (BW > MAX_WEIGHT)
            reason = "Too high weight";

        // Write diagnostic info
        debug_file << person.id() << "," << person.age << ","
                   << (person.gender == core::Gender::male ? "M" : "F") << "," << reason << ","
                   << BW_0 << "," << BW << "," << F << "," << L << "," << G << "," << W << ","
                   << ECF << "," << PAL << "," << EI << "," << CI << "," << Na << "," << delta
                   << "," << tau << "," << steady_F << "," << steady_L << "," << F_0 << "," << L_0
                   << std::endl;

        // Clamp to valid range
        if (std::isnan(BW) || std::isinf(BW)) {
            // Use expected value from table
            BW =
                get_expected(*context_, person.gender, person.age, "Weight"_id, std::nullopt, true);
            // Also reset component values proportionally
            double ratio = BW / (F_0 + L_0 + G + W + ECF_0);
            F = F_0 * ratio;
            L = L_0 * ratio;
        } else {
            // For out-of-range values, use capping with age adjustment
            if (person.age > 80) {
                // Older people naturally have lower BMI - adjust max weight down
                double age_factor = 1.0 - std::min(0.3, (person.age - 80) * 0.015);
                MAX_WEIGHT *= age_factor;
            }
            BW = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, BW));

            // Adjust F and L proportionally if needed
            if (F < 0)
                F = 0.2 * BW; // Minimum healthy body fat
            if (L < 0)
                L = 0.6 * BW; // Minimum lean mass

            // Ensure components sum to BW
            double components_sum = F + L + G + W + ECF;
            if (components_sum > 0) {
                double ratio = BW / components_sum;
                F *= ratio;
                L *= ratio;
            } else {
                // Fallback if components are invalid
                F = 0.2 * BW;
                L = 0.6 * BW;
                G = 0.01 * BW;
                W = 2.7 * G;
                ECF = 0.7 * 0.235 * BW;
            }
        }
    }

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
    // Use ranges from the configuration (factors in the static_linear_model.cpp contains these
    // values) Just hard coding in to make sure- MAHIMA Don't worry the code will only use the
    // config values
    double MIN_WEIGHT = 1.0;   // Default minimum weight (kg)
    double MAX_WEIGHT = 210.0; // Default maximum weight (kg)
    double MIN_HEIGHT = 44.0;  // Default minimum height (cm)
    double MAX_HEIGHT = 203.2; // Default maximum height (cm)
    double MIN_BMI = 14.26126; // Default minimum BMI
    double MAX_BMI = 45.84575; // Default maximum BMI

    // Get actual ranges from config if available
    if (nutrient_ranges_.contains("Weight"_id)) {
        const auto &weight_range = nutrient_ranges_.at("Weight"_id);
        MIN_WEIGHT = weight_range.lower();
        MAX_WEIGHT = weight_range.upper();
    }

    if (nutrient_ranges_.contains("Height"_id)) {
        const auto &height_range = nutrient_ranges_.at("Height"_id);
        MIN_HEIGHT = height_range.lower();
        MAX_HEIGHT = height_range.upper();
    }

    if (nutrient_ranges_.contains("BMI"_id)) {
        const auto &bmi_range = nutrient_ranges_.at("BMI"_id);
        MIN_BMI = bmi_range.lower();
        MAX_BMI = bmi_range.upper();
    }

    // Get weight and height, with validation
    double w = person.risk_factors.at("Weight"_id);
    double h = person.risk_factors.at("Height"_id);

    // Log extreme values
    bool logged = false;
    std::string reason;

    if (std::isnan(w) || std::isinf(w) || w < MIN_WEIGHT || w > MAX_WEIGHT) {
        reason = "Invalid weight: " + std::to_string(w);
        std::cerr << "WARNING: Invalid weight " << w << " for person " << person.id() << " (age "
                  << person.age << ", gender " << (person.gender == core::Gender::male ? "M" : "F")
                  << ")" << std::endl;
        logged = true;

        // Clamp to valid range
        if (std::isnan(w) || std::isinf(w)) {
            // Use expected value from table
            w = get_expected(*context_, person.gender, person.age, "Weight"_id, std::nullopt, true);
        } else {
            w = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, w));
        }

        // Update the person's weight
        person.risk_factors["Weight"_id] = w;
    }

    if (std::isnan(h) || std::isinf(h) || h < MIN_HEIGHT || h > MAX_HEIGHT) {
        reason = reason.empty() ? "" : reason + "; ";
        reason += "Invalid height: " + std::to_string(h);
        std::cerr << "WARNING: Invalid height " << h << " for person " << person.id() << " (age "
                  << person.age << ", gender " << (person.gender == core::Gender::male ? "M" : "F")
                  << ")" << std::endl;
        logged = true;

        // Clamp to valid range
        if (std::isnan(h) || std::isinf(h)) {
            // Use expected value from table
            h = get_expected(*context_, person.gender, person.age, "Height"_id, std::nullopt,
                             false);
        } else {
            h = std::max(MIN_HEIGHT, std::min(MAX_HEIGHT, h));
        }

        // Update the person's height
        person.risk_factors["Height"_id] = h;
    }

    // Calculate BMI with validated values
    double bmi = w / pow(h / 100.0, 2);

    // Validate BMI result
    if (std::isnan(bmi) || std::isinf(bmi) || bmi < MIN_BMI || bmi > MAX_BMI) {
        reason = reason.empty() ? "" : reason + "; ";
        reason += "Invalid BMI: " + std::to_string(bmi) + " (w/h²=" + std::to_string(w) + "/" +
                  std::to_string(h) + "²)";

        if (!logged) {
            std::cerr << "WARNING: Invalid BMI " << bmi << " calculated for person " << person.id()
                      << " (age " << person.age << ", gender "
                      << (person.gender == core::Gender::male ? "M" : "F") << ", weight " << w
                      << ", height " << h << ")" << std::endl;
        }

        // Set to a reasonable value based on age, sex
        bmi = 22.0; // Default reasonable BMI

        if (person.gender == core::Gender::male) {
            if (person.age < 18)
                bmi = 19.0;
            else if (person.age > 65)
                bmi = 24.0;
        } else {
            if (person.age < 18)
                bmi = 18.0;
            else if (person.age > 65)
                bmi = 25.0;
        }

        // Ensure BMI is within range
        bmi = std::max(MIN_BMI, std::min(MAX_BMI, bmi));

        // Log details to CSV for debugging
        log_invalid_bmi_to_csv(person, w, h, bmi, reason);
    }

    // Set validated BMI
    person.risk_factors["BMI"_id] = bmi;
}

void KevinHallModel::initialise_weight(RuntimeContext &context, Person &person) const {
    // Get weight range from config
    double MIN_WEIGHT = 1.0;   // Default minimum weight (kg)
    double MAX_WEIGHT = 210.0; // Default maximum weight (kg)

    // Get actual ranges from config if available
    if (nutrient_ranges_.contains("Weight"_id)) {
        const auto &weight_range = nutrient_ranges_.at("Weight"_id);
        MIN_WEIGHT = weight_range.lower();
        MAX_WEIGHT = weight_range.upper();
    }

    try {
        // Compute E/PA expected.
        double ei_expected =
            get_expected(context, person.gender, person.age, "EnergyIntake"_id, std::nullopt, true);
        double pa_expected = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                          std::nullopt, false);

        // Safety check for physical activity
        if (pa_expected <= 0.01 || std::isnan(pa_expected) || std::isinf(pa_expected)) {
            // Only log truly invalid values, not just out-of-range ones
            if (std::isnan(pa_expected) || std::isinf(pa_expected)) {
                std::cerr << "WARNING: Invalid physical activity value detected" << std::endl;
            }
            pa_expected = 1.5; // Reasonable default value
        }

        double epa_expected = ei_expected / pa_expected;

        // Safety check for expected energy intake
        if (std::isnan(ei_expected) || std::isinf(ei_expected) || ei_expected <= 0) {
            // Only log truly invalid values, not just out-of-range ones
            if (std::isnan(ei_expected) || std::isinf(ei_expected)) {
                std::cerr << "WARNING: Invalid energy intake value detected" << std::endl;
            }

            // Use reasonable defaults based on age and gender
            if (person.age < 2) {
                ei_expected = 800.0; // Infant
            } else if (person.age < 10) {
                ei_expected = 1500.0; // Child
            } else if (person.age < 18) {
                ei_expected = person.gender == core::Gender::male ? 2500.0 : 2000.0; // Teenager
            } else {
                ei_expected = person.gender == core::Gender::male ? 2600.0 : 2000.0; // Adult
            }
            epa_expected = ei_expected / pa_expected;
        }

        // Compute E/PA actual.
        double ei_actual = person.risk_factors.at("EnergyIntake"_id);
        double pa_actual = person.risk_factors.at("PhysicalActivity"_id);

        // Safety check for actual physical activity
        if (pa_actual <= 0.01 || std::isnan(pa_actual) || std::isinf(pa_actual)) {
            // Only log truly invalid values, not just out-of-range ones
            if (std::isnan(pa_actual) || std::isinf(pa_actual)) {
                std::cerr << "WARNING: Invalid physical activity value detected" << std::endl;
            }
            pa_actual = pa_expected; // Use expected value as fallback
            person.risk_factors["PhysicalActivity"_id] = pa_actual;
        }

        // Safety check for actual energy intake
        if (std::isnan(ei_actual) || std::isinf(ei_actual) || ei_actual <= 0) {
            // Only log truly invalid values, not just out-of-range ones
            if (std::isnan(ei_actual) || std::isinf(ei_actual)) {
                std::cerr << "WARNING: Invalid energy intake value detected" << std::endl;
            }
            ei_actual = ei_expected; // Use expected value as fallback
            person.risk_factors["EnergyIntake"_id] = ei_actual;
        }

        double epa_actual = ei_actual / pa_actual;

        // Compute E/PA quantile with safety check
        double epa_quantile = epa_actual / epa_expected;
        if (std::isnan(epa_quantile) || std::isinf(epa_quantile) || epa_quantile <= 0 ||
            epa_quantile > 10) {
            // Only log truly invalid values, not just out-of-range ones
            if (std::isnan(epa_quantile) || std::isinf(epa_quantile)) {
                std::cerr << "WARNING: Invalid EPA quantile value detected" << std::endl;
            }
            epa_quantile = 1.0; // Use neutral quantile as fallback
        }

        // Compute new weight.
        double w_expected =
            get_expected(context, person.gender, person.age, "Weight"_id, std::nullopt, true);

        // Safety check for expected weight
        if (std::isnan(w_expected) || std::isinf(w_expected) || w_expected < MIN_WEIGHT ||
            w_expected > MAX_WEIGHT) {
            // Only log truly invalid values, not just out-of-range ones
            if (std::isnan(w_expected) || std::isinf(w_expected)) {
                std::cerr << "WARNING: Invalid expected weight value detected" << std::endl;
            }

            // Use reasonable defaults based on age and gender
            if (person.age < 1) {
                w_expected = 4.0 + (person.age * 6.0); // Infant (grows from ~4kg to ~10kg)
            } else if (person.age < 10) {
                w_expected = 10.0 + ((person.age - 1) * 3.0); // Child (grows ~3kg per year)
            } else if (person.age < 18) {
                if (person.gender == core::Gender::male) {
                    w_expected =
                        30.0 + ((person.age - 10) * 5.0); // Male teen (grows ~5kg per year)
                } else {
                    w_expected =
                        30.0 + ((person.age - 10) * 4.0); // Female teen (grows ~4kg per year)
                }
            } else {
                // Adult
                w_expected = person.gender == core::Gender::male ? 70.0 : 60.0;
            }

            // Ensure within range
            w_expected = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, w_expected));
        }

        double w_quantile = get_weight_quantile(epa_quantile, person.gender);

        // Safety check for weight quantile
        if (std::isnan(w_quantile) || std::isinf(w_quantile) || w_quantile <= 0 || w_quantile > 5) {
            // Only log truly invalid values, not just out-of-range ones
            if (std::isnan(w_quantile) || std::isinf(w_quantile)) {
                std::cerr << "WARNING: Invalid weight quantile value detected" << std::endl;
            }
            w_quantile = 1.0; // Use neutral quantile as fallback
        }

        double final_weight = w_expected * w_quantile;

        // Final safety check on calculated weight - silently clamp out-of-range values
        if (std::isnan(final_weight) || std::isinf(final_weight)) {
            // Only log truly invalid values
            std::cerr << "WARNING: Invalid weight calculation (NaN or infinity)" << std::endl;
            final_weight = w_expected; // Fallback to expected weight
        } else if (final_weight < MIN_WEIGHT || final_weight > MAX_WEIGHT) {
            // Silently clamp to range without warning messages
            final_weight = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, w_expected));
        }

        person.risk_factors["Weight"_id] = final_weight;
    } catch (const std::exception &e) {
        // Handle any exceptions during weight calculation
        std::cerr << "ERROR: Exception in initialise_weight: " << e.what() << std::endl;

        // Set a reasonable default weight based on age and gender
        double default_weight;
        if (person.age < 1) {
            default_weight = 4.0 + (person.age * 6.0); // Infant
        } else if (person.age < 10) {
            default_weight = 10.0 + ((person.age - 1) * 3.0); // Child
        } else if (person.age < 18) {
            if (person.gender == core::Gender::male) {
                default_weight = 30.0 + ((person.age - 10) * 5.0); // Male teen
            } else {
                default_weight = 30.0 + ((person.age - 10) * 4.0); // Female teen
            }
        } else {
            // Adult
            default_weight = person.gender == core::Gender::male ? 70.0 : 60.0;
        }

        // Ensure within range
        default_weight = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, default_weight));

        person.risk_factors["Weight"_id] = default_weight;
    }
}

void KevinHallModel::adjust_weight(Person &person, double adjustment) const {
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
    size_t weight_index_last = weight_quantiles_.at(sex).size() - 1;
    auto weight_index = static_cast<size_t>(epa_percentile * weight_index_last);
    return weight_quantiles_.at(sex)[weight_index];
}

KevinHallAdjustmentTable
KevinHallModel::compute_mean_weight(Population &population,
                                    std::optional<std::unordered_map<core::Gender, double>> power,
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
        // Ignore if inactive or not the optionally specified age.
        if (!person.is_active()) {
            continue;
        }
        if (age.has_value() && person.age != age.value()) {
            continue;
        }

        double value;
        if (power.has_value()) {
            value = pow(person.risk_factors.at("Weight"_id), power.value()[person.gender]);
        } else {
            value = person.risk_factors.at("Weight"_id);
        }

        sumcounts.at(person.gender)[person.age].append(value);
    }

    // Compute means of weight powers for sex and age.
    auto means = KevinHallAdjustmentTable{};
    for (const auto &[sumcount_sex, sumcounts_by_sex] : sumcounts) {
        means.emplace_row(sumcount_sex, std::unordered_map<int, double>{});
        for (const auto &[sumcount_age, sumcount] : sumcounts_by_sex) {
            means.at(sumcount_sex)[sumcount_age] = sumcount.mean();
        }
    }

    return means;
}

void KevinHallModel::initialise_height(RuntimeContext &context, Person &person, double W_power_mean,
                                       Random &random) const {

    // Initialise lifelong height residual.
    double H_residual = random.next_normal(0, height_stddev_.at(person.gender));
    person.risk_factors["Height_residual"_id] = H_residual;

    // Initialise height.
    update_height(context, person, W_power_mean);
}

void KevinHallModel::update_height(RuntimeContext &context, Person &person,
                                   double W_power_mean) const {
    // Get height range from config
    double MIN_HEIGHT = 44.0;  // Default minimum height (cm)
    double MAX_HEIGHT = 203.2; // Default maximum height (cm)

    // Get actual ranges from config if available
    if (nutrient_ranges_.contains("Height"_id)) {
        const auto &height_range = nutrient_ranges_.at("Height"_id);
        MIN_HEIGHT = height_range.lower();
        MAX_HEIGHT = height_range.upper();
    }

    // Get weight range from config
    double MIN_WEIGHT = 1.0;   // Default
    double MAX_WEIGHT = 210.0; // Default

    if (nutrient_ranges_.contains("Weight"_id)) {
        const auto &weight_range = nutrient_ranges_.at("Weight"_id);
        MIN_WEIGHT = weight_range.lower();
        MAX_WEIGHT = weight_range.upper();
    }

    try {
        // Check weight is valid
        double W = person.risk_factors.at("Weight"_id);
        if (std::isnan(W) || std::isinf(W) || W < MIN_WEIGHT || W > MAX_WEIGHT) {
            std::cerr << "WARNING: Invalid weight (" << W << ") for person " << person.id()
                      << " in update_height, using expected value" << std::endl;
            // Use expected value from table
            W = get_expected(context, person.gender, person.age, "Weight"_id, std::nullopt, true);
            // Ensure within range
            W = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, W));
            // Update the person's weight
            person.risk_factors["Weight"_id] = W;
        }

        // Validate W_power_mean
        if (std::isnan(W_power_mean) || std::isinf(W_power_mean) || W_power_mean <= 0) {
            std::cerr << "WARNING: Invalid W_power_mean (" << W_power_mean << ") for person "
                      << person.id() << " in update_height, using weight directly" << std::endl;
            // Use a fallback calculation
            W_power_mean = W;
        }

        double H_expected =
            get_expected(context, person.gender, person.age, "Height"_id, std::nullopt, false);

        // Validate H_expected
        if (std::isnan(H_expected) || std::isinf(H_expected) || H_expected < MIN_HEIGHT ||
            H_expected > MAX_HEIGHT) {
            std::cerr << "WARNING: Invalid expected height (" << H_expected << ") for person "
                      << person.id() << " in update_height, using default value" << std::endl;

            // Use reasonable defaults based on age and gender
            if (person.age < 1) {
                H_expected = 50.0 + (person.age * 20.0); // Infant (50-70cm)
            } else if (person.age < 10) {
                H_expected = 70.0 + ((person.age - 1) * 8.0); // Child (~8cm per year)
            } else if (person.age < 18) {
                if (person.gender == core::Gender::male) {
                    H_expected = 140.0 + ((person.age - 10) * 6.0); // Male teen (~6cm per year)
                } else {
                    H_expected = 140.0 + ((person.age - 10) * 5.0); // Female teen (~5cm per year)
                }
            } else {
                // Adult
                H_expected = person.gender == core::Gender::male ? 175.0 : 163.0;
            }

            // Ensure within range
            H_expected = std::max(MIN_HEIGHT, std::min(MAX_HEIGHT, H_expected));
        }

        // Rest of the function remains the same
        double H_residual = 0.0;
        if (person.risk_factors.contains("Height_residual"_id)) {
            H_residual = person.risk_factors.at("Height_residual"_id);
            // Validate H_residual
            if (std::isnan(H_residual) || std::isinf(H_residual) || H_residual < -5.0 ||
                H_residual > 5.0) {
                std::cerr << "WARNING: Invalid height residual (" << H_residual << ") for person "
                          << person.id() << " in update_height, using default value" << std::endl;
                H_residual = 0.0;
                person.risk_factors["Height_residual"_id] = H_residual;
            }
        } else {
            // If Height_residual doesn't exist, initialize it
            std::cerr << "WARNING: Missing Height_residual for person " << person.id()
                      << " in update_height, using default value" << std::endl;
            H_residual = 0.0;
            person.risk_factors["Height_residual"_id] = H_residual;
        }

        // Safely get stddev and slope
        double stddev = 0.01; // Default value
        double slope = 0.01;  // Default value

        try {
            stddev = height_stddev_.at(person.gender);
            slope = height_slope_.at(person.gender);

            // Validate stddev and slope
            if (std::isnan(stddev) || std::isinf(stddev) || stddev <= 0) {
                std::cerr << "WARNING: Invalid height stddev (" << stddev << ") for person "
                          << person.id() << " in update_height, using default value" << std::endl;
                stddev = 0.01;
            }

            if (std::isnan(slope) || std::isinf(slope) || slope <= 0) {
                std::cerr << "WARNING: Invalid height slope (" << slope << ") for person "
                          << person.id() << " in update_height, using default value" << std::endl;
                slope = 0.01;
            }
        } catch (const std::out_of_range &) {
            std::cerr << "ERROR: Missing height stddev/slope for gender "
                      << (person.gender == core::Gender::male ? "male" : "female")
                      << " in update_height, using default values" << std::endl;
        }

        // Safely compute height
        double exp_norm_mean = exp(0.5 * stddev * stddev);

        // Safety check for exp_norm_mean
        if (std::isnan(exp_norm_mean) || std::isinf(exp_norm_mean) || exp_norm_mean <= 0) {
            std::cerr << "WARNING: Invalid exp_norm_mean (" << exp_norm_mean << ") for person "
                      << person.id() << " in update_height, using default value" << std::endl;
            exp_norm_mean = 1.0;
        }

        // Safely compute power term
        double power_term = 1.0;
        try {
            power_term = pow(W, slope) / W_power_mean;

            // Safety check for power term
            if (std::isnan(power_term) || std::isinf(power_term) || power_term <= 0 ||
                power_term > 10.0) {
                std::cerr << "WARNING: Invalid power term (" << power_term << ") for person "
                          << person.id() << " in update_height, using default value" << std::endl;
                power_term = 1.0;
            }
        } catch (const std::exception &e) {
            std::cerr << "ERROR: Exception computing power term in update_height for person "
                      << person.id() << ": " << e.what() << ", using default value" << std::endl;
        }

        // Safely compute residual term
        double residual_term = 1.0;
        try {
            residual_term = exp(H_residual) / exp_norm_mean;

            // Safety check for residual term
            if (std::isnan(residual_term) || std::isinf(residual_term) || residual_term <= 0 ||
                residual_term > 10.0) {
                std::cerr << "WARNING: Invalid residual term (" << residual_term << ") for person "
                          << person.id() << " in update_height, using default value" << std::endl;
                residual_term = 1.0;
            }
        } catch (const std::exception &e) {
            std::cerr << "ERROR: Exception computing residual term in update_height for person "
                      << person.id() << ": " << e.what() << ", using default value" << std::endl;
        }

        // Compute final height
        double H = H_expected * power_term * residual_term;

        // Final validation of height
        if (std::isnan(H) || std::isinf(H) || H < MIN_HEIGHT || H > MAX_HEIGHT) {
            std::cerr << "WARNING: Final height calculation invalid (" << H << ") for person "
                      << person.id() << " in update_height, using expected height" << std::endl;
            H = H_expected;
        }

        // Set height
        person.risk_factors["Height"_id] = H;

    } catch (const std::exception &e) {
        // Handle any unexpected exceptions
        std::cerr << "ERROR: Exception in update_height for person " << person.id() << ": "
                  << e.what() << std::endl;

        // Set a reasonable default height based on age and gender
        double default_height;
        if (person.age < 1) {
            default_height = 50.0 + (person.age * 20.0); // Infant
        } else if (person.age < 10) {
            default_height = 70.0 + ((person.age - 1) * 8.0); // Child
        } else if (person.age < 18) {
            if (person.gender == core::Gender::male) {
                default_height = 140.0 + ((person.age - 10) * 6.0); // Male teen
            } else {
                default_height = 140.0 + ((person.age - 10) * 5.0); // Female teen
            }
        } else {
            // Adult
            default_height = person.gender == core::Gender::male ? 175.0 : 163.0;
        }

        // Ensure within range
        default_height = std::max(MIN_HEIGHT, std::min(MAX_HEIGHT, default_height));

        person.risk_factors["Height"_id] = default_height;
    }
}

// Modified: Mahima 25/02/2025
// Region is initialised using the CDF of the region probabilities along with age/gender strata
void KevinHallModel::initialise_region(RuntimeContext &context, Person &person,
                                       Random &random) const {
    // Delegate to demographic module
    context.demographic_module().initialise_region(context, person, random);
}

// NOTE: Might need to change this if region updates are happening differently
void KevinHallModel::update_region([[maybe_unused]] RuntimeContext &context, Person &person,
                                   Random &random) const {
    if (person.age == 18) {
        initialise_region(context, person, random);
    }
}

// Modified: Mahima 25/02/2025
// Ethnicity is initialised using the CDF of the ethnicity probabilities along with
// age/gender/region strata
void KevinHallModel::initialise_ethnicity(RuntimeContext &context, Person &person,
                                          Random &random) const {
    // Delegate to demographic module
    context.demographic_module().initialise_ethnicity(context, person, random);
}
// NOTE: No update ethnicity as it is fixed throughout once assigned

// Modified: Mahima 25/02/2025
// Physical activity is initialised using the expected value of physical activity based on age,
// gender, region, ethnicity and income
void KevinHallModel::initialise_physical_activity(RuntimeContext &context, Person &person,
                                                  Random &random) const {
    // Delegate to demographic module
    context.demographic_module().initialise_physical_activity(context, person, random);
}

// Modified: Mahima 25/02/2025
// Income is initialised using the SoftMax of the income probabilities based on age, gender, region,
// ethnicity
// this uses a logistic regression model to predict the income category
void KevinHallModel::initialise_income_continuous(Person &person, Random &random) const {
    // Delegate to demographic module
    context_->demographic_module().initialise_income_continuous(person, random);
}

void KevinHallModel::update_income_continuous(Person &person, Random &random) const {
    // Removing age check to ensure income is updated for all individuals regardless of age
    // This treats income as household income rather than individual income

    // Call the income initialization function
    initialise_income_continuous(person, random);
}

// Modified: Mahima 25/02/2025, Optimized for large populations
// Helper function to calculate income thresholds once for the entire population
std::tuple<double, double, double>
KevinHallModel::calculate_income_thresholds(const Population &population) const {
    return context_->demographic_module().calculate_income_thresholds(population);
}

// Modified: Mahima 25/02/2025, Optimized for large populations
// Income category is initialised using the quartiles of the income_continuous values
// This method now only assigns the category based on pre-calculated thresholds
void KevinHallModel::initialise_income_category(Person &person, double q1_threshold,
                                                double q2_threshold, double q3_threshold) const {
    // Delegate to demographic module
    context_->demographic_module().initialise_income_category(person, q1_threshold, q2_threshold,
                                                              q3_threshold);
}

// Modified to calculate thresholds once for the entire population
void KevinHallModel::update_income_category(RuntimeContext &context) const {
    static int last_update_year = 0;
    int current_year = context.time_now();

    // Update quartiles every 5 years
    if (current_year - last_update_year >= 5) {
        // Calculate thresholds once for the entire population
        auto [q1_threshold, q2_threshold, q3_threshold] =
            calculate_income_thresholds(context.population());

        std::cout << "INFO: Updating income categories with thresholds Q1=" << q1_threshold
                  << ", Q2=" << q2_threshold << ", Q3=" << q3_threshold << std::endl;

        // Validate thresholds are properly ordered
        if (q1_threshold > q2_threshold || q2_threshold > q3_threshold) {
            std::cerr
                << "ERROR: Income thresholds are incorrectly ordered in update_income_category"
                << std::endl;
            // Fix by using percentages of range
            double min_income = 23.0;
            double max_income = 2375.0;
            double range = max_income - min_income;
            q1_threshold = min_income + range * 0.25;
            q2_threshold = min_income + range * 0.5;
            q3_threshold = min_income + range * 0.75;
            std::cerr << "Using fixed thresholds instead: Q1=" << q1_threshold
                      << ", Q2=" << q2_threshold << ", Q3=" << q3_threshold << std::endl;
        }

        // Track inconsistencies during update
        int inconsistency_count = 0;
        int fixed_count = 0;

        // Apply thresholds to each person
        for (auto &person : context.population()) {
            if (person.is_active()) {
                // Store original category for comparison
                auto original_category = person.income_category;

                // Get income value and ensure it's within valid range
                double income_value = std::max(23.0, std::min(2375.0, person.income_continuous));

                // Check if income continuous and category are consistent
                bool is_consistent = true;
                if ((person.income_category == core::Income::low && income_value >= q1_threshold) ||
                    (person.income_category == core::Income::lowermiddle &&
                     (income_value < q1_threshold || income_value >= q2_threshold)) ||
                    (person.income_category == core::Income::uppermiddle &&
                     (income_value < q2_threshold || income_value >= q3_threshold)) ||
                    (person.income_category == core::Income::high && income_value < q3_threshold)) {
                    is_consistent = false;
                    inconsistency_count++;

                    // Resynchronize by updating the category based on income value
                    initialise_income_category(person, q1_threshold, q2_threshold, q3_threshold);
                    fixed_count++;

                    // Debug specific cases for first few inconsistencies
                    if (inconsistency_count <= 5) {
                        std::cout << "FIXED: Person " << person.id() << " Income=" << income_value;

                        std::cout << ", Category changed from ";
                        switch (original_category) {
                        case core::Income::low:
                            std::cout << "Low";
                            break;
                        case core::Income::lowermiddle:
                            std::cout << "Lower Middle";
                            break;
                        case core::Income::uppermiddle:
                            std::cout << "Upper Middle";
                            break;
                        case core::Income::high:
                            std::cout << "High";
                            break;
                        default:
                            std::cout << "Unknown";
                        }

                        std::cout << " to ";
                        switch (person.income_category) {
                        case core::Income::low:
                            std::cout << "Low";
                            break;
                        case core::Income::lowermiddle:
                            std::cout << "Lower Middle";
                            break;
                        case core::Income::uppermiddle:
                            std::cout << "Upper Middle";
                            break;
                        case core::Income::high:
                            std::cout << "High";
                            break;
                        default:
                            std::cout << "Unknown";
                        }
                        std::cout << std::endl;
                    }
                } else {
                    // Already consistent, no action needed
                }

                // Special rule for edge cases - very low income should never be High
                if (income_value <= 30.0 && person.income_category == core::Income::high) {
                    person.income_category = core::Income::low;
                    std::cout << "OVERRIDE: Forcing Low category for very low income ("
                              << income_value << ") for person " << person.id() << std::endl;
                }

                // Special rule for edge cases - very high income should never be Low
                if (income_value >= 2300.0 && person.income_category == core::Income::low) {
                    person.income_category = core::Income::high;
                    std::cout << "OVERRIDE: Forcing High category for very high income ("
                              << income_value << ") for person " << person.id() << std::endl;
                }
            }
        }

        if (inconsistency_count > 0) {
            std::cout << "INFO: Fixed " << fixed_count << " of " << inconsistency_count
                      << " income category inconsistencies ("
                      << (100.0 * inconsistency_count / context.population().current_active_size())
                      << "% of active population)" << std::endl;
        }

        last_update_year = current_year;
    }
}

KevinHallModelDefinition::KevinHallModelDefinition(
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    std::unordered_map<core::Identifier, double> energy_equation,
    std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges,
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
    std::unordered_map<core::Identifier, std::optional<double>> food_prices,
    std::unordered_map<core::Gender, std::vector<double>> weight_quantiles,
    std::vector<double> epa_quantiles, std::unordered_map<core::Gender, double> height_stddev,
    std::unordered_map<core::Gender, double> height_slope,
    std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models,
    std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> ethnicity_models,
    std::unordered_map<core::Income, LinearModelParams> income_models,
    double income_continuous_stddev)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps)},
      energy_equation_{std::move(energy_equation)}, nutrient_ranges_{std::move(nutrient_ranges)},
      nutrient_equations_{std::move(nutrient_equations)}, food_prices_{std::move(food_prices)},
      weight_quantiles_{std::move(weight_quantiles)}, epa_quantiles_{std::move(epa_quantiles)},
      height_stddev_{std::move(height_stddev)}, height_slope_{std::move(height_slope)},
      region_models_{std::move(region_models)}, ethnicity_models_{std::move(ethnicity_models)},
      income_models_{std::move(income_models)},
      income_continuous_stddev_{income_continuous_stddev} {

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
    if (height_slope_.empty()) {
        throw core::HgpsException("Height slope mapping is empty");
    }
}

std::unique_ptr<RiskFactorModel> KevinHallModelDefinition::create_model() const {
    return std::make_unique<KevinHallModel>(
        std::make_shared<RiskFactorSexAgeTable>(*expected_),
        std::make_shared<std::unordered_map<core::Identifier, double>>(*expected_trend_),
        std::make_shared<std::unordered_map<core::Identifier, int>>(*trend_steps_),
        energy_equation_, nutrient_ranges_, nutrient_equations_, food_prices_, weight_quantiles_,
        epa_quantiles_, height_stddev_, height_slope_, region_models_, ethnicity_models_,
        income_models_, income_continuous_stddev_);
}

} // namespace hgps
// NOLINTEND(readability-convert-member-functions-to-static)
