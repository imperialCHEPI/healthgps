#include "HealthGPS.Core/exception.h"

#include "kevin_hall_model.h"
#include "runtime_context.h"
#include "sync_message.h"

#include <algorithm>
#include <iostream>
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
    const std::unordered_map<core::Gender, double> &height_slope)
    : RiskFactorAdjustableModel{std::move(expected), std::move(expected_trend),
                                std::move(trend_steps)},
      energy_equation_{energy_equation}, nutrient_ranges_{nutrient_ranges},
      nutrient_equations_{nutrient_equations}, food_prices_{food_prices},
      weight_quantiles_{weight_quantiles}, epa_quantiles_{epa_quantiles},
      height_stddev_{height_stddev}, height_slope_{height_slope} {

    // Print nutrient ranges to verify they're loaded correctly
    std::cout << "\n======= LOADED NUTRIENT RANGES IN KEVIN HALL =======";
    bool weight_range_found = false;
    for (const auto &[key, range] : nutrient_ranges_) {
        std::cout << "\nNutrient: " << key.to_string() << ", Range: [" << range.lower() << " , "
                  << range.upper() << "]";
        if (key == "Weight"_id) {
            weight_range_found = true;
            std::cout << " <--- WEIGHT RANGE FOUND!";
        }
    }

    if (!weight_range_found) {
        std::cout << "\n!!! WARNING: WEIGHT RANGE NOT FOUND IN NUTRIENT RANGES !!!";
    } else {
        std::cout << "\n*** Weight range is defined and will be used for clamping ***";
    }
    std::cout << "\n=====================================\n";
}

RiskFactorModelType KevinHallModel::type() const noexcept { return RiskFactorModelType::Dynamic; }

std::string KevinHallModel::name() const noexcept { return "Dynamic"; }

void KevinHallModel::generate_risk_factors(RuntimeContext &context) {
    // std::cout << "\nDEBUG: KevinHallModel::generate_risk_factors - Starting" << std::endl;

    // Initialise everyone.
    for (auto &person : context.population()) {
        initialise_nutrient_intakes(person);
        initialise_energy_intake(person);
        initialise_weight(context, person);
    }

    // Adjust weight mean to match expected.
    // Create a vector of ranges with the Weight range if available
    std::vector<core::DoubleInterval> weight_ranges;
    if (nutrient_ranges_.contains("Weight"_id)) {
        weight_ranges.push_back(nutrient_ranges_.at("Weight"_id));
        adjust_risk_factors(context, {"Weight"_id}, weight_ranges, true);
    } else {
        // Fall back to std::nullopt if no Weight range is defined
        adjust_risk_factors(context, {"Weight"_id}, std::nullopt, true);
    }

    // Print weight values for a sample of people after adjustment
    std::cout << "\n===== WEIGHT ADJUSTMENT CHECK: SAMPLE OF 5 PEOPLE =====";
    int sample_count = 0;
    int print_interval = std::max(1, static_cast<int>(context.population().size() / 5));
    for (const auto &person : context.population()) {
        if (!person.is_active())
            continue;

        if (person.id() % print_interval == 0 && sample_count < 5) {
            std::cout << "\nPerson ID: " << person.id() << ", Age: " << person.age
                      << ", Gender: " << (person.gender == core::Gender::male ? "Male" : "Female")
                      << ", Weight: " << person.risk_factors.at("Weight"_id) << " kg";
            sample_count++;
        }

        if (sample_count >= 5)
            break;
    }
    std::cout << "\n================================================\n";

    // Compute weight power means by sex and age.
    auto W_power_means = compute_mean_weight(context.population(), height_slope_);

    // Initialise everyone.
    for (auto &person : context.population()) {
        double W_power_mean = W_power_means.at(person.gender, person.age);
        initialise_height(context, person, W_power_mean, context.random());
        initialise_kevin_hall_state(person);
        compute_bmi(person);
    }
}

void KevinHallModel::update_risk_factors(RuntimeContext &context) {
    // std::cout << "\nDEBUG: KevinHallModel::update_risk_factors - Starting" << std::endl;

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
    }
}

void KevinHallModel::update_newborns(RuntimeContext &context) const {
    // std::cout << "\nDEBUG: KevinHallModel::update_newborns - Starting" << std::endl;

    // Initialise nutrient and energy intake and weight for newborns.
    for (auto &person : context.population()) {
        // Ignore if inactive or not newborn.
        if (!person.is_active() || (person.age != 0)) {
            continue;
        }

        initialise_nutrient_intakes(person);
        initialise_energy_intake(person);
        initialise_weight(context, person);
    }

    // TODO: This newborn adjustment needs sending to intervention scenario -- see #266.

    // NOTE: FOR REFACTORING: This block should eventually be replaced by a call to
    // `adjust_risk_factors(context, {"Weight"_id}, IntegerInterval{0, 0});` once age_range
    // is implemented in the adjustment method, as it is redundant and does not communicate
    // the newborn weight adjustment to the intervention (see #266).
    // NOTE: FOR REFACTORING: When implementing this, ensure the Weight range from nutrient_ranges_
    // is passed to prevent NaN values, similar to generate_risk_factors implementation.

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

        // Ensure weight is within valid range
        if (nutrient_ranges_.contains("Weight"_id)) {
            double current_weight = person.risk_factors.at("Weight"_id);
            double clamped_weight = nutrient_ranges_.at("Weight"_id).clamp(current_weight);
            person.risk_factors.at("Weight"_id) = clamped_weight;
        }
    }

    // NOTE: FOR REFACTORING: End of semi-redundant block.

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
// NOLINTBEGIN(readability-function-cognitive-complexity)
void KevinHallModel::update_non_newborns(RuntimeContext &context) const {
    // std::cout << "\nDEBUG: KevinHallModel::update_non_newborns - Starting" << std::endl;

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
            // Apply the adjustment
            adjust_weight(person, adjustment);

            // Ensure the final weight is within valid range
            if (nutrient_ranges_.contains("Weight"_id)) {
                double current_weight = person.risk_factors.at("Weight"_id);
                double clamped_weight =
                    std::clamp(current_weight, nutrient_ranges_.at("Weight"_id).lower(),
                               nutrient_ranges_.at("Weight"_id).upper());
                person.risk_factors.at("Weight"_id) = clamped_weight;
            }
        }
    }

    // Print weight values for a sample of people after adjustment
    std::cout << "\n===== WEIGHT ADJUSTMENT CHECK DURING UPDATE: SAMPLE OF 5 PEOPLE =====";
    std::cout << "\nYear: " << context.time_now();
    int sample_count = 0;
    int print_interval = std::max(1, static_cast<int>(context.population().size() / 5));
    for (const auto &person : context.population()) {
        if (!person.is_active() || person.age == 0)
            continue;

        if (person.id() % print_interval == 0 && sample_count < 5) {
            double adjustment_value = 0.0;
            if (adjustments.contains(person.gender, person.age)) {
                adjustment_value = adjustments.at(person.gender, person.age);
            }

            std::cout << "\nPerson ID: " << person.id() << ", Age: " << person.age
                      << ", Gender: " << (person.gender == core::Gender::male ? "Male" : "Female")
                      << ", Weight: " << person.risk_factors.at("Weight"_id) << " kg"
                      << ", Adjustment: " << adjustment_value;
            sample_count++;
        }

        if (sample_count >= 5)
            break;
    }
    std::cout << "\n=================================================================\n";

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
} // NOLINTEND(readability-function-cognitive-complexity)

KevinHallAdjustmentTable KevinHallModel::receive_weight_adjustments(RuntimeContext &context) const {
    // std::cout << "\nDEBUG: KevinHallModel::receive_weight_adjustments - Starting" << std::endl;
    KevinHallAdjustmentTable adjustments;

    // Baseline scenatio: compute adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        return compute_weight_adjustments(context);
    }

    // Intervention scenario: receive adjustments from baseline scenario.
    // Initialize message with a value that has_value() will return false for
    std::optional<std::unique_ptr<SyncMessage>> message;

    // Keep trying until we get a message
    do {
        message = context.scenario().channel().try_receive(context.sync_timeout_millis());
    } while (!message.has_value());

    // Keep trying until we get a message of the correct type
    auto *messagePrt = dynamic_cast<KevinHallAdjustmentMessage *>(message.value().get());

    while (!messagePrt) {
        // Initialize message again to avoid uninitialized variable warning
        do {
            message = context.scenario().channel().try_receive(context.sync_timeout_millis());
        } while (!message.has_value());

        messagePrt = dynamic_cast<KevinHallAdjustmentMessage *>(message.value().get());
    }

    return messagePrt->data();
}

void KevinHallModel::send_weight_adjustments(RuntimeContext &context,
                                             KevinHallAdjustmentTable &&adjustments) const {
    // std::cout << "\nDEBUG: KevinHallModel::send_weight_adjustments - Starting" << std::endl;
    //  Baseline scenario: send adjustments.
    if (context.scenario().type() == ScenarioType::baseline) {
        context.scenario().channel().send(std::make_unique<KevinHallAdjustmentMessage>(
            context.current_run(), context.time_now(), std::move(adjustments)));
    }
}

KevinHallAdjustmentTable
KevinHallModel::compute_weight_adjustments(RuntimeContext &context,
                                           std::optional<unsigned> age) const {
    // std::cout << "\nDEBUG: KevinHallModel::compute_weight_adjustments - Starting" << std::endl;
    auto W_means = compute_mean_weight(context.population(), std::nullopt, age);

    // Compute adjustments.
    auto adjustments = KevinHallAdjustmentTable{};
    for (const auto &[W_mean_sex, W_means_by_sex] : W_means) {
        adjustments.emplace_row(W_mean_sex, std::unordered_map<int, double>{});
        for (const auto &[W_mean_age, W_mean] : W_means_by_sex) {
            // NOTE: we only have the ages we requested here.
            double W_expected =
                get_expected(context, W_mean_sex, W_mean_age, "Weight"_id, std::nullopt, true);

            // NaN check for weight calculations
            if (std::isnan(W_expected) || std::isnan(W_mean)) {
                // std::cout << "\nWARNING: NaN detected in weight adjustment calculation for "<<
                // "age=" << W_mean_age << ". Using 0 adjustment.";
                adjustments.at(W_mean_sex)[W_mean_age] = 0.0;
                continue;
            }
            double adjustment = W_expected - W_mean;

            adjustments.at(W_mean_sex)[W_mean_age] = adjustment;
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

void KevinHallModel::initialise_nutrient_intakes(Person &person) const {
    // Set all nutrients to valid initial values before computing
    for (const auto &[nutrient_key, unused] : energy_equation_) {
        // Initialize to minimum valid value if range exists
        if (nutrient_ranges_.contains(nutrient_key)) {
            person.risk_factors[nutrient_key] = nutrient_ranges_.at(nutrient_key).lower();
        } else {
            person.risk_factors[nutrient_key] = 0.0;
        }
    }

    // Now compute nutrient intakes
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
    // Reset nutrient intakes to zero
    for (const auto &[nutrient_key, unused] : energy_equation_) {
        person.risk_factors[nutrient_key] = 0.0;
    }

    // First, compute all nutrient intakes from food intakes WITHOUT clamping
    for (const auto &[food_key, nutrient_coefficients] : nutrient_equations_) {
        double food_intake = person.risk_factors.at(food_key);
        // Ensure food intake is not negative
        food_intake = std::max(0.0, food_intake);

        for (const auto &[nutrient_key, nutrient_coefficient] : nutrient_coefficients) {
            double new_value =
                person.risk_factors.at(nutrient_key) + (food_intake * nutrient_coefficient);
            person.risk_factors.at(nutrient_key) = new_value;
        }
    }

    // Second, apply clamping to ensure values are within valid ranges
    for (const auto &[nutrient_key, unused] : energy_equation_) {
        if (nutrient_ranges_.contains(nutrient_key)) {
            double current_value = person.risk_factors.at(nutrient_key);
            double clamped_value = nutrient_ranges_.at(nutrient_key).clamp(current_value);
            person.risk_factors.at(nutrient_key) = clamped_value;
        }
    }

    // Quick verification - only print if something is out of range
    for (const auto &[nutrient_key, unused] : energy_equation_) {
        if (nutrient_ranges_.contains(nutrient_key)) {
            double value = person.risk_factors.at(nutrient_key);
            const auto &range = nutrient_ranges_.at(nutrient_key);
            if (value < range.lower() || value > range.upper()) {
                std::cout << "\nNUTRIENT RANGE ERROR: Person " << person.id() << " has "
                          << nutrient_key.to_string() << "=" << value << " outside valid range ["
                          << range.lower() << ", " << range.upper() << "]";
                // break; // Print only first error to keep output minimal
            }
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
    // std::cout << "\nDEBUG: KevinHallModel::initialise_kevin_hall_state - Starting for person #"
    // << person.id() << std::endl;
    //  Apply optional weight adjustment.
    if (adjustment.has_value()) {
        double adjusted_weight = person.risk_factors.at("Weight"_id) + adjustment.value();

        // Clamp weight within valid range if it exists in nutrient_ranges_
        if (nutrient_ranges_.contains("Weight"_id)) {
            adjusted_weight = nutrient_ranges_.at("Weight"_id).clamp(adjusted_weight);
        }

        person.risk_factors.at("Weight"_id) = adjusted_weight;
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

    // Debug NaN check
    if (std::isnan(BW_0)) {
        std::cout << "\nDEBUG NaN DETECTED in kevin_hall_run - Initial weight is NaN for person:"
                  << "\n  ID: " << person.id() << "\n  Age: " << person.age
                  << "\n  Gender: " << (person.gender == core::Gender::male ? "Male" : "Female")
                  << "\n  Previous weight: " << BW_0;
        std::exit(1);
    }

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

    // Check for division by zero in denominator
    double denominator = (a1 * b2 - a2 * b1);
    if (std::abs(denominator) < 1e-10) {
        // Log the issue
        std::cout << "\nWARNING: Near-zero denominator detected in kevin_hall_run for person ID: " 
                  << person.id() << ". Using fallback values.";
        
        // Use F_0 and L_0 as the final values
        double F = F_0;
        double L = L_0;
        
        // Compute body weight with the fallback values
        double BW = F + L + G + W + ECF;
        if (nutrient_ranges_.contains("Weight"_id)) {
            BW = nutrient_ranges_.at("Weight"_id).clamp(BW);
        }
        
        // Set the state values
        person.risk_factors.at("Glycogen"_id) = G;
        person.risk_factors.at("ExtracellularFluid"_id) = ECF;
        person.risk_factors.at("BodyFat"_id) = F;
        person.risk_factors.at("LeanTissue"_id) = L;
        person.risk_factors.at("Weight"_id) = BW;
        
        return; // Exit the function early
    }
    
    // Compute body fat and lean tissue steady state.
    double steady_F = -(b1 * c2 - b2 * c1) / denominator;
    double steady_L = -(c1 * a2 - c2 * a1) / denominator;

    // Extra safety check for steady state values
    if (!std::isfinite(steady_F) || !std::isfinite(steady_L)) {
        std::cout << "\nWARNING: Non-finite steady state values detected in final F/L calculation for person ID: " 
                  << person.id() << ". Using fallback values.";
        
        // Use previous values as fallback
        steady_F = F_0;
        steady_L = L_0;
    }

    // Compute time constant with safety check
    double tau_denominator = ((gamma_F + delta) * (1.0 - p) * rho_L + (gamma_L + delta) * p * rho_F);
    if (std::abs(tau_denominator) < 1e-10) {
        std::cout << "\nWARNING: Near-zero denominator in tau calculation for person ID: " 
                  << person.id() << ". Using fallback values.";
        
        // Use previous values as fallback
        double F = F_0;
        double L = L_0;
        
        // Compute body weight with the fallback values
        double BW = F + L + G + W + ECF;
        if (nutrient_ranges_.contains("Weight"_id)) {
            BW = nutrient_ranges_.at("Weight"_id).clamp(BW);
        }
        
        // Set the state values
        person.risk_factors.at("Glycogen"_id) = G;
        person.risk_factors.at("ExtracellularFluid"_id) = ECF;
        person.risk_factors.at("BodyFat"_id) = F;
        person.risk_factors.at("LeanTissue"_id) = L;
        person.risk_factors.at("Weight"_id) = BW;
        
        return; // Exit the function early
    }

    double tau = rho_L * rho_F * (1.0 + x) / tau_denominator;

    // Safety check for tau value
    if (tau < 0.0 || !std::isfinite(tau)) {
        // Use previous values as fallback
        double F = F_0;
        double L = L_0;
        
        // Compute body weight with the fallback values
        double BW = F + L + G + W + ECF;
        if (nutrient_ranges_.contains("Weight"_id)) {
            BW = nutrient_ranges_.at("Weight"_id).clamp(BW);
        }
        
        // Set the state values
        person.risk_factors.at("Glycogen"_id) = G;
        person.risk_factors.at("ExtracellularFluid"_id) = ECF;
        person.risk_factors.at("BodyFat"_id) = F;
        person.risk_factors.at("LeanTissue"_id) = L;
        person.risk_factors.at("Weight"_id) = BW;
        
        return; // Exit the function early
    }

    // Safer exponential calculation
    double exp_arg = -365.0 / tau;
    // Limit extreme negative values to prevent underflow
    if (exp_arg < -700.0) {
        exp_arg = -700.0; // Limit to avoid underflow
    }
    double exp_term = exp(exp_arg);

    // Compute body fat and lean tissue with safer exponential term
    double F = steady_F - (steady_F - F_0) * exp_term;
    double L = steady_L - (steady_L - L_0) * exp_term;

    // Extra safety check for F and L - if they're not finite after calculation, use previous values
    if (!std::isfinite(F) || !std::isfinite(L)) {
        std::cout << "\nWARNING: Non-finite values detected in final F/L calculation for person ID: " 
                  << person.id() << ". Using fallback values.";
        
        // Use previous values as fallback
        F = F_0;
        L = L_0;
    }

    // Compute body weight.
    double BW = F + L + G + W + ECF;

    // Debug NaN check
    if (std::isnan(BW) || !std::isfinite(BW)) {
        std::cout << "\nDEBUG NaN DETECTED in kevin_hall_run - Final weight calculation:"
                  << "\n  Person ID: " << person.id() << "\n  Age: " << person.age
                  << "\n  Gender: " << (person.gender == core::Gender::male ? "Male" : "Female")
                  << "\n  Initial EI: " << EI_0 << "\n  Current EI: " << EI
                  << "\n  Delta EI: " << delta_EI << "\n  TEF: " << TEF << "\n  AT: " << AT
                  << "\n  Initial carb: " << CI_0 << "\n  Current carb: " << CI
                  << "\n  Initial weight: " << BW_0 << "\n  Current weight: " << BW
                  << "\n  Body fat: " << F << "\n  Lean tissue: " << L << "\n  Previous G: " << G_0
                  << "\n  Glycogen: " << G << "\n  Water: " << W << "\n  ECF: " << ECF
                  << "\n  Energy intake: " << EI << "\n  Physical activity: " << PAL
                  << "\n  Height: " << H;

        // Exit to help identify NaN sources
        std::exit(1);
    }

    // Clamp weight within valid range if it exists in nutrient_ranges_
    if (nutrient_ranges_.contains("Weight"_id)) {
        BW = nutrient_ranges_.at("Weight"_id).clamp(BW);
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
    auto w = person.risk_factors.at("Weight"_id);
    auto h = person.risk_factors.at("Height"_id) / 100;

    // Calculate BMI
    double bmi = w / (h * h);
    person.risk_factors["BMI"_id] = bmi;
}

void KevinHallModel::initialise_weight(RuntimeContext &context, Person &person) const {
    // Compute E/PA expected.
    double ei_expected =
        get_expected(context, person.gender, person.age, "EnergyIntake"_id, std::nullopt, true);
    double pa_expected = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                      std::nullopt, false);
    double epa_expected = ei_expected / pa_expected;

    // Compute E/PA actual.
    double ei_actual = person.risk_factors.at("EnergyIntake"_id);
    double pa_actual = person.risk_factors.at("PhysicalActivity"_id);
    double epa_actual = ei_actual / pa_actual;

    // Compute E/PA quantile.
    double epa_quantile = epa_actual / epa_expected;

    // Compute new weight.
    double w_expected =
        get_expected(context, person.gender, person.age, "Weight"_id, std::nullopt, true);
    double w_quantile = get_weight_quantile(epa_quantile, person.gender);
    double weight = w_expected * w_quantile;

    // Debug NaN check
    if (std::isnan(weight)) {
        std::cout << "\nDEBUG NaN DETECTED in initialise_weight:"
                  << "\n  Person ID: " << person.id() << "\n  Age: " << person.age
                  << "\n  Gender: " << (person.gender == core::Gender::male ? "Male" : "Female")
                  << "\n  Expected weight: " << w_expected << "\n  Weight quantile: " << w_quantile
                  << "\n  Energy intake expected: " << ei_expected
                  << "\n  Physical activity expected: " << pa_expected
                  << "\n  Energy intake actual: " << ei_actual
                  << "\n  Physical activity actual: " << pa_actual
                  << "\n  EPA quantile: " << epa_quantile;
        std::exit(1);
    }

    // Clamp weight within valid range if it exists in nutrient_ranges_
    if (nutrient_ranges_.contains("Weight"_id)) {
        weight = nutrient_ranges_.at("Weight"_id).clamp(weight);
    }

    person.risk_factors["Weight"_id] = weight;
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

    // Clamp weight within valid range if it exists in nutrient_ranges_
    if (nutrient_ranges_.contains("Weight"_id)) {
        BW = nutrient_ranges_.at("Weight"_id).clamp(BW);
    }

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
    // std::cout << "\nDEBUG: KevinHallModel::compute_mean_weight - Starting" << std::endl;
    //  Local struct to hold count and sum of weight powers.
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
    double W = person.risk_factors.at("Weight"_id);
    double H_expected =
        get_expected(context, person.gender, person.age, "Height"_id, std::nullopt, false);
    double H_residual = person.risk_factors.at("Height_residual"_id);
    double stddev = height_stddev_.at(person.gender);
    double slope = height_slope_.at(person.gender);

    // Compute height.
    double exp_norm_mean = exp(0.5 * stddev * stddev);
    double H = H_expected * (pow(W, slope) / W_power_mean) * (exp(H_residual) / exp_norm_mean);

    // Set height (may not exist yet).
    person.risk_factors["Height"_id] = H;
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
    std::unordered_map<core::Gender, double> height_slope)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps)},
      energy_equation_{std::move(energy_equation)}, nutrient_ranges_{std::move(nutrient_ranges)},
      nutrient_equations_{std::move(nutrient_equations)}, food_prices_{std::move(food_prices)},
      weight_quantiles_{std::move(weight_quantiles)}, epa_quantiles_{std::move(epa_quantiles)},
      height_stddev_{std::move(height_stddev)}, height_slope_{std::move(height_slope)} {

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
    return std::make_unique<KevinHallModel>(expected_, expected_trend_, trend_steps_,
                                            energy_equation_, nutrient_ranges_, nutrient_equations_,
                                            food_prices_, weight_quantiles_, epa_quantiles_,
                                            height_stddev_, height_slope_);
}

} // namespace hgps
// NOLINTEND(readability-convert-member-functions-to-static)
