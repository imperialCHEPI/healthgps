#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/income_category_layout.h"

#include "kevin_hall_model.h"
#include "runtime_context.h"
#include "sync_message.h"

#include <algorithm>
#include <fmt/core.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <syncstream>
#include <unordered_map>
#include <utility>

namespace { // anonymous namespace

using KevinHallAdjustmentMessage =
    hgps::SyncDataMessage<hgps::UnorderedMap2d<hgps::core::Gender, int, double>>;

std::string income_category_label(hgps::core::Income income) {
    switch (income) {
    case hgps::core::Income::low:
        return "low";
    case hgps::core::Income::lowermiddle:
        return "lowermiddle";
    case hgps::core::Income::middle:
        return "middle";
    case hgps::core::Income::uppermiddle:
        return "uppermiddle";
    case hgps::core::Income::high:
        return "high";
    default:
        return "unknown";
    }
}

std::string gender_label(const hgps::Person &person) {
    if (person.gender == hgps::core::Gender::male) {
        return "male";
    }
    if (person.gender == hgps::core::Gender::female) {
        return "female";
    }
    return "unknown";
}

std::string sector_label(hgps::core::Sector sector) {
    switch (sector) {
    case hgps::core::Sector::urban:
        return "0";
    case hgps::core::Sector::rural:
        return "1";
    default:
        return "unknown";
    }
}

std::string format_weight_range_violation_message(const hgps::RuntimeContext &context,
                                                  const hgps::Person &person,
                                                  std::string_view phase, double weight,
                                                  const hgps::core::DoubleInterval &range,
                                                  bool above_maximum) {
    const char *bound_phrase =
        above_maximum ? "above maximum configured range" : "below minimum configured range";
    std::ostringstream message;
    message << "Weight (" << std::fixed << std::setprecision(6) << weight << " kg) is "
            << bound_phrase << " [" << range.lower() << ", " << range.upper()
            << "] kg during phase '" << phase << "'.\n"
            << "  person_id=" << person.id() << "\n"
            << "  age=" << person.age << " years\n"
            << "  gender=" << gender_label(person) << "\n"
            << "  region=" << person.region << "\n"
            << "  ethnicity=" << person.ethnicity << "\n"
            << "  sector=" << sector_label(person.sector) << "\n"
            << "  income_category=" << income_category_label(person.income) << "\n"
            << "  income_continuous=" << std::setprecision(6) << person.income_continuous << "\n"
            << "  simulation_time=" << context.time_now() << "\n"
            << "  simulation_run=" << context.current_run() << "\n"
            << "  scenario=" << context.scenario().name() << "\n"
            << "  active=" << (person.is_active() ? "true" : "false");

    const auto append_if_present = [&](const char *key, const char *label, bool as_cm = false) {
        const hgps::core::Identifier id(key);
        if (!person.risk_factors.contains(id)) {
            return;
        }
        const double value = person.risk_factors.at(id);
        message << "\n  " << label << '=';
        if (as_cm) {
            message << std::setprecision(6) << (value * 100.0);
        } else {
            message << std::setprecision(6) << value;
        }
    };

    append_if_present("Height", "height_cm", true);
    append_if_present("BMI", "bmi");
    append_if_present("EnergyIntake", "energy_intake");
    append_if_present("PhysicalActivity", "physical_activity");

    return message.str();
}

bool should_print_kevin_hall_summary_tables(const hgps::RuntimeContext &context) {
    // MAHIMA: Keep console reporting limited to baseline start and first update year so
    // diagnostics remain useful without flooding long simulation logs.
    if (context.scenario().type() != hgps::ScenarioType::baseline) {
        return false;
    }
    const int current_year = context.time_now();
    const int start_year = context.start_time();
    return current_year == start_year || current_year == (start_year + 1);
}

struct HeightBucketSummary {
    std::size_t count{0};
    double slope_sum{0.0};
    double std_sum{0.0};
    double height_min{std::numeric_limits<double>::max()};
    double height_max{std::numeric_limits<double>::lowest()};
    double height_sum{0.0};
};

const std::vector<double> &resolve_weight_quantiles_for_person(
    const hgps::Person &person,
    const std::unordered_map<hgps::core::Gender, std::vector<std::vector<double>>>
        &weight_quantiles_by_stratum) {
    const auto by_gender = weight_quantiles_by_stratum.find(person.gender);
    if (by_gender == weight_quantiles_by_stratum.end() || by_gender->second.empty()) {
        throw hgps::core::HgpsException("Weight quantiles missing for person gender");
    }
    const auto &params = by_gender->second;
    if (!person.has_income_adjustment_stratum) {
        return params.front();
    }
    const std::size_t index = std::min(person.income_adjustment_stratum, params.size() - 1);
    return params[index];
}

const hgps::HeightModelParams &resolve_height_params_for_person(
    const hgps::Person &person,
    const std::unordered_map<hgps::core::Gender, std::vector<hgps::HeightModelParams>>
        &height_params) {
    // MAHIMA: Height slope/stddev are selected by gender first, then by income adjustment
    // stratum when available (single-row CSV is treated as broadcast via params.front()).
    const auto by_gender = height_params.find(person.gender);
    if (by_gender == height_params.end() || by_gender->second.empty()) {
        throw hgps::core::HgpsException("Height model parameters missing for person gender");
    }
    const auto &params = by_gender->second;
    if (!person.has_income_adjustment_stratum) {
        return params.front();
    }
    const std::size_t index = std::min(person.income_adjustment_stratum, params.size() - 1);
    return params[index];
}

void print_height_stratum_assignment_table(
    const hgps::Population &population,
    const std::unordered_map<hgps::core::Gender, std::vector<hgps::HeightModelParams>>
        &height_params,
    std::size_t bucket_count, int year, std::string_view phase) {
    if (bucket_count == 0) {
        return;
    }

    const hgps::core::Identifier height_id("Height");
    std::vector<HeightBucketSummary> summaries(bucket_count);
    for (const auto &person : population) {
        if (!person.is_active() || !person.has_income_adjustment_stratum ||
            person.income_adjustment_stratum >= bucket_count) {
            continue;
        }
        auto height_it = person.risk_factors.find(height_id);
        if (height_it == person.risk_factors.end()) {
            continue;
        }

        const auto &height_model = resolve_height_params_for_person(person, height_params);
        auto &summary = summaries[person.income_adjustment_stratum];
        summary.count++;
        summary.slope_sum += height_model.slope;
        summary.std_sum += height_model.stddev;
        const double height = height_it->second;
        summary.height_sum += height;
        summary.height_min = std::min(summary.height_min, height);
        summary.height_max = std::max(summary.height_max, height);
    }

    std::ostringstream out;
    out << "\n[HEIGHT STRATUM ASSIGNMENT] Year " << year << " phase=" << phase
        << " (uses person.income_adjustment_stratum from static model)\n";
    out << "+--------+----------------------+-------+-------------+-------------+-------------+"
           "-------------+-------------+\n";
    out << "| Bucket | Stratum ID           | Count | Slope       | StdDev      | Height Min  | "
           "Height Max  | Height Mean |\n";
    out << "+--------+----------------------+-------+-------------+-------------+-------------+"
           "-------------+-------------+\n";
    for (std::size_t k = 0; k < bucket_count; ++k) {
        const auto &summary = summaries[k];
        const std::string stratum_id = "Quintile" + std::to_string(k + 1);
        out << "| " << std::setw(6) << k << " | " << std::setw(20) << std::left << stratum_id
            << std::right << " | " << std::setw(5) << summary.count << " | ";
        if (summary.count == 0) {
            out << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a";
        } else {
            const auto n = static_cast<double>(summary.count);
            out << std::setw(11) << fmt::format("{:.6f}", summary.slope_sum / n) << " | "
                << std::setw(11) << fmt::format("{:.6f}", summary.std_sum / n) << " | "
                << std::setw(11) << fmt::format("{:.3f}", summary.height_min) << " | "
                << std::setw(11) << fmt::format("{:.3f}", summary.height_max) << " | "
                << std::setw(11) << fmt::format("{:.3f}", summary.height_sum / n);
        }
        out << " |\n";
    }
    out << "+--------+----------------------+-------+-------------+-------------+-------------+"
           "-------------+-------------+\n";
    // MAHIMA: Avoid std::osyncstream here — it can block when gtest redirects stdout
    // (CaptureStdout) in unit tests, making the suite appear hung.
    std::cout << out.str() << std::flush;
}

struct WeightBucketSummary {
    std::size_t count{0};
    std::size_t quantile_size{0};
    double weight_min{std::numeric_limits<double>::max()};
    double weight_max{std::numeric_limits<double>::lowest()};
    double weight_sum{0.0};
};

void print_weight_stratum_assignment_table(
    const hgps::Population &population,
    const std::unordered_map<hgps::core::Gender, std::vector<std::vector<double>>>
        &weight_quantiles_by_stratum,
    std::size_t bucket_count, int year, std::string_view phase) {
    if (bucket_count == 0) {
        return;
    }

    const hgps::core::Identifier weight_id("Weight");
    std::vector<WeightBucketSummary> summaries(bucket_count);
    for (const auto &person : population) {
        if (!person.is_active() || !person.has_income_adjustment_stratum ||
            person.income_adjustment_stratum >= bucket_count) {
            continue;
        }
        auto weight_it = person.risk_factors.find(weight_id);
        if (weight_it == person.risk_factors.end()) {
            continue;
        }

        const auto &quantiles =
            resolve_weight_quantiles_for_person(person, weight_quantiles_by_stratum);
        auto &summary = summaries[person.income_adjustment_stratum];
        summary.count++;
        summary.quantile_size = quantiles.size();
        const double weight = weight_it->second;
        summary.weight_sum += weight;
        summary.weight_min = std::min(summary.weight_min, weight);
        summary.weight_max = std::max(summary.weight_max, weight);
    }

    std::ostringstream out;
    out << "\n[WEIGHT STRATUM ASSIGNMENT] Year " << year << " phase=" << phase
        << " (uses person.income_adjustment_stratum from static model)\n";
    out << "+--------+----------------------+-------+---------------+-------------+-------------+"
           "-------------+\n";
    out << "| Bucket | Stratum ID           | Count | Quantile Size | Weight Min  | Weight Max  | "
           "Weight Mean |\n";
    out << "+--------+----------------------+-------+---------------+-------------+-------------+"
           "-------------+\n";
    for (std::size_t k = 0; k < bucket_count; ++k) {
        const auto &summary = summaries[k];
        const std::string stratum_id = "Quintile" + std::to_string(k + 1);
        out << "| " << std::setw(6) << k << " | " << std::setw(20) << std::left << stratum_id
            << std::right << " | " << std::setw(5) << summary.count << " | ";
        if (summary.count == 0) {
            out << std::setw(13) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a";
        } else {
            const auto n = static_cast<double>(summary.count);
            out << std::setw(13) << summary.quantile_size << " | " << std::setw(11)
                << fmt::format("{:.3f}", summary.weight_min) << " | " << std::setw(11)
                << fmt::format("{:.3f}", summary.weight_max) << " | " << std::setw(11)
                << fmt::format("{:.3f}", summary.weight_sum / n);
        }
        out << " |\n";
    }
    out << "+--------+----------------------+-------+---------------+-------------+-------------+"
           "-------------+\n";
    std::cout << out.str() << std::flush;
}

void print_weight_by_final_income_category_table(
    const hgps::Population &population, const hgps::core::IncomeCategoryLayout &layout, int year,
    std::string_view phase) {
    const hgps::core::Identifier weight_id("Weight");
    std::vector<WeightBucketSummary> summaries(layout.count);

    for (const auto &person : population) {
        if (!person.is_active()) {
            continue;
        }
        auto weight_it = person.risk_factors.find(weight_id);
        if (weight_it == person.risk_factors.end()) {
            continue;
        }

        std::size_t idx = 0;
        try {
            idx = hgps::core::income_table_index(person.income, layout);
        } catch (const hgps::core::HgpsException &) {
            continue;
        }

        auto &summary = summaries[idx];
        summary.count++;
        const double weight = weight_it->second;
        summary.weight_sum += weight;
        summary.weight_min = std::min(summary.weight_min, weight);
        summary.weight_max = std::max(summary.weight_max, weight);
    }

    std::ostringstream out;
    out << "\n[WEIGHT BY FINAL INCOME CATEGORY] Year " << year << " phase=" << phase
        << " project_requirements.categories=" << layout.count
        << " (person.income set by static model after quintile adjustment)\n";
    out << "+----------+--------+-------------+-------------+-------------+\n";
    out << "| Category | Count  | Weight Min  | Weight Max  | Weight Mean |\n";
    out << "+----------+--------+-------------+-------------+-------------+\n";
    for (std::size_t i = 0; i < summaries.size(); ++i) {
        const auto &summary = summaries[i];
        out << "| " << std::setw(8) << std::left << layout.labels[i] << std::right << " | "
            << std::setw(6)
            << summary.count << " | ";
        if (summary.count == 0) {
            out << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a";
        } else {
            out << std::setw(11) << fmt::format("{:.3f}", summary.weight_min) << " | "
                << std::setw(11) << fmt::format("{:.3f}", summary.weight_max) << " | "
                << std::setw(11)
                << fmt::format("{:.3f}", summary.weight_sum / static_cast<double>(summary.count));
        }
        out << " |\n";
    }
    out << "+----------+--------+-------------+-------------+-------------+\n";
    std::cout << out.str() << std::flush;
}

void print_height_by_final_income_category_table(
    const hgps::Population &population, const hgps::core::IncomeCategoryLayout &layout, int year,
    std::string_view phase) {
    const hgps::core::Identifier height_id("Height");
    std::vector<HeightBucketSummary> summaries(layout.count);

    for (const auto &person : population) {
        if (!person.is_active()) {
            continue;
        }
        auto height_it = person.risk_factors.find(height_id);
        if (height_it == person.risk_factors.end()) {
            continue;
        }

        std::size_t idx = 0;
        try {
            idx = hgps::core::income_table_index(person.income, layout);
        } catch (const hgps::core::HgpsException &) {
            continue;
        }

        auto &summary = summaries[idx];
        summary.count++;
        const double height = height_it->second;
        summary.height_sum += height;
        summary.height_min = std::min(summary.height_min, height);
        summary.height_max = std::max(summary.height_max, height);
    }

    std::ostringstream out;
    out << "\n[HEIGHT BY FINAL INCOME CATEGORY] Year " << year << " phase=" << phase
        << " project_requirements.categories=" << layout.count
        << " (person.income set by static model after quintile adjustment)\n";
    out << "+----------+--------+-------------+-------------+-------------+\n";
    out << "| Category | Count  | Height Min  | Height Max  | Height Mean |\n";
    out << "+----------+--------+-------------+-------------+-------------+\n";
    for (std::size_t i = 0; i < summaries.size(); ++i) {
        const auto &summary = summaries[i];
        out << "| " << std::setw(8) << std::left << layout.labels[i] << std::right << " | "
            << std::setw(6)
            << summary.count << " | ";
        if (summary.count == 0) {
            out << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a";
        } else {
            out << std::setw(11) << fmt::format("{:.3f}", summary.height_min) << " | "
                << std::setw(11) << fmt::format("{:.3f}", summary.height_max) << " | "
                << std::setw(11)
                << fmt::format("{:.3f}", summary.height_sum / static_cast<double>(summary.count));
        }
        out << " |\n";
    }
    out << "+----------+--------+-------------+-------------+-------------+\n";
    std::cout << out.str() << std::flush;
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
    const std::unordered_map<core::Gender, std::vector<std::vector<double>>>
        &weight_quantiles_by_stratum,
    const std::vector<double> &epa_quantiles,
    std::unordered_map<core::Gender, std::vector<HeightModelParams>> height_params)
    : RiskFactorAdjustableModel{std::move(expected), std::move(expected_trend),
                                std::move(trend_steps)},
      energy_equation_{energy_equation}, nutrient_ranges_{nutrient_ranges},
      nutrient_equations_{nutrient_equations}, food_prices_{food_prices},
      weight_quantiles_by_stratum_{weight_quantiles_by_stratum}, epa_quantiles_{epa_quantiles},
      height_params_{std::move(height_params)} {}

RiskFactorModelType KevinHallModel::type() const noexcept { return RiskFactorModelType::Dynamic; }

std::string KevinHallModel::name() const noexcept { return "Dynamic"; }

void KevinHallModel::generate_risk_factors(RuntimeContext &context) {

    // Initialise everyone.
    for (auto &person : context.population()) {
        initialise_nutrient_intakes(person);
        initialise_energy_intake(person);
        initialise_weight(context, person);
    }

    // Adjust weight mean to match expected.
    adjust_risk_factors(context, {"Weight"_id}, std::nullopt, true);
    // MAHIMA: Validate weight after adjusting the weight mean to match expected. This is done after
    // adjusting the weight mean to match expected to ensure that the weight is within the
    // configured range.
    for (auto &person : context.population()) {
        if (person.is_active()) {
            validate_weight_in_config_range(context, person,
                                            "generate_risk_factors:after_weight_mean_adjustment");
        }
    }

    // Compute weight power means by sex and age.
    auto W_power_means = compute_mean_weight_for_height(context.population());

    // Initialise everyone.
    for (auto &person : context.population()) {
        double W_power_mean = W_power_means.at(person.gender, person.age);
        initialise_height(context, person, W_power_mean, context.random());
        initialise_kevin_hall_state(context, person);
        compute_bmi(person);
    }

    print_height_summary_tables(context, "generate");
    print_weight_summary_tables(context, "generate");
}

void KevinHallModel::update_risk_factors(RuntimeContext &context) {

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
        validate_weight_in_config_range(context, person,
                                        "update_newborns:after_newborn_weight_adjustment");
    }

    // NOTE: FOR REFACTORING: End of semi-redundant block.

    // Compute newborn weight power means by sex.
    auto W_power_means = compute_mean_weight_for_height(context.population(), 0);

    // Initialise height and other Kevin Hall state for newborns.
    for (auto &person : context.population()) {
        // Ignore if inactive or not newborn.
        if (!person.is_active() || (person.age != 0)) {
            continue;
        }

        double W_power_mean = W_power_means.at(person.gender, person.age);
        initialise_height(context, person, W_power_mean, context.random());
        initialise_kevin_hall_state(context, person);
    }

    print_height_summary_tables(context, "update-newborns");
    print_weight_summary_tables(context, "update-newborns");
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
            kevin_hall_run(context, person);
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
            initialise_kevin_hall_state(context, person, adjustment);
        } else {
            adjust_weight(context, person, adjustment);
        }
    }

    // Send (baseline) weight adjustments to intervention scenario.
    send_weight_adjustments(context, std::move(adjustments));

    // Compute weight power means by sex and age.
    auto W_power_means = compute_mean_weight_for_height(context.population());

    // Update: (no newborns or at least the Kevin Hall minimum age).
    for (auto &person : context.population()) {
        // Ignore if inactive or newborn or at least the Kevin Hall minimum age.
        if (!person.is_active() || ((person.age == 0) || (person.age >= kevin_hall_age_min))) {
            continue;
        }

        double W_power_mean = W_power_means.at(person.gender, person.age);
        update_height(context, person, W_power_mean);
    }

    print_height_summary_tables(context, "update-children");
    print_weight_summary_tables(context, "update-children");
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
                                    bool apply_trend) const {

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

        // MAHIMA: Calculate PAL-based coefficient from factors mean
        // Replaces hardcoded 0.06256 with 1/(9.99 * PA) where PA is person-specific physical
        // activity
        double pa_from_factors_mean =
            get_expected(context, sex, age, "PhysicalActivity"_id, std::nullopt, false);
        double pal_coefficient = 1.0 / (9.99 * pa_from_factors_mean);
        weight += pal_coefficient *
                  get_expected(context, sex, age, "EnergyIntake"_id, std::nullopt, true);
        weight -= 0.6256 * get_expected(context, sex, age, "Height"_id, std::nullopt, false);
        weight += 0.4925 * age;

        // MAHIMA: Gender coefficient fix - assign to males instead of females
        // Due to enum mismatch where female=1 instead of male=1
        double gender_coefficient = (sex == core::Gender::male) ? 16.6166 : 0.0;
        weight -= gender_coefficient;

        return weight;
    }

    // Fall back to base class implementation for other factors.
    return RiskFactorAdjustableModel::get_expected(context, sex, age, factor, range, apply_trend);
}

void KevinHallModel::initialise_nutrient_intakes(Person &person) const {

    // Initialise nutrient intakes.
    compute_nutrient_intakes(person);

    // Start with previous = current.
    if (!person.risk_factors.contains("Carbohydrate"_id)) {
        std::cout << "\nERROR: Missing 'Carbohydrate' in initialise_nutrient_intakes";
        throw;
    }
    if (!person.risk_factors.contains("Sodium"_id)) {
        std::cout << "\nERROR: Missing 'Sodium' in initialise_nutrient_intakes";
        throw;
    }
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
        if (!person.risk_factors.contains(food_key)) {
            std::cout << "\nERROR: Missing risk factor key in Kevin Hall model: '"
                      << food_key.to_string() << "' for person (age=" << person.age
                      << ", gender=" << (person.gender == core::Gender::male ? "male" : "female")
                      << ")";
            throw;
        }
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
    if (!person.risk_factors.contains("EnergyIntake"_id)) {
        std::cout << "\nERROR: Missing 'EnergyIntake' in initialise_energy_intake";
        throw;
    }
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
        if (!person.risk_factors.contains(nutrient_key)) {
            std::cout << "\nERROR: Missing nutrient key '" << nutrient_key.to_string()
                      << "' in compute_energy_intake for person (age=" << person.age
                      << ", gender=" << (person.gender == core::Gender::male ? "male" : "female")
                      << ")";
            throw;
        }
        double nutrient_intake = person.risk_factors.at(nutrient_key);
        person.risk_factors.at(energy_intake_key) += nutrient_intake * energy_coefficient;
    }
}

void KevinHallModel::initialise_kevin_hall_state(const RuntimeContext &context, Person &person,
                                                 std::optional<double> adjustment) const {

    // Apply optional weight adjustment.
    if (adjustment.has_value()) {
        person.risk_factors.at("Weight"_id) += adjustment.value();
        validate_weight_in_config_range(context, person,
                                        "initialise_kevin_hall_state:after_weight_adjustment");
    }

    // Get already computed values.
    if (!person.risk_factors.contains("Height"_id)) {
        std::cout << "\nERROR: Missing 'Height' in initialise_kevin_hall_state";
        throw;
    }
    if (!person.risk_factors.contains("Weight"_id)) {
        std::cout << "\nERROR: Missing 'Weight' in initialise_kevin_hall_state";
        throw;
    }
    if (!person.risk_factors.contains("PhysicalActivity"_id)) {
        std::cout << "\nERROR: Missing 'PhysicalActivity' in initialise_kevin_hall_state";
        throw;
    }
    if (!person.risk_factors.contains("EnergyIntake"_id)) {
        std::cout << "\nERROR: Missing 'EnergyIntake' in initialise_kevin_hall_state";
        throw;
    }
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

void KevinHallModel::kevin_hall_run(const RuntimeContext &context, Person &person) const {

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
    validate_weight_in_config_range(context, person, "kevin_hall_run");
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

void KevinHallModel::initialise_weight(RuntimeContext &context, Person &person) const {
    // Compute E/PA expected.
    double ei_expected =
        get_expected(context, person.gender, person.age, "EnergyIntake"_id, std::nullopt, true);
    double pa_expected = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                      std::nullopt, false);
    double epa_expected = ei_expected / pa_expected;

    // Compute E/PA actual.
    if (!person.risk_factors.contains("EnergyIntake"_id)) {
        std::cout << "\nERROR: Missing 'EnergyIntake' in initialise_weight for person (age="
                  << person.age
                  << ", gender=" << (person.gender == core::Gender::male ? "male" : "female")
                  << ")";
        throw;
    }
    if (!person.risk_factors.contains("PhysicalActivity"_id)) {
        std::cout << "\nERROR: Missing 'PhysicalActivity' in initialise_weight for person (age="
                  << person.age
                  << ", gender=" << (person.gender == core::Gender::male ? "male" : "female")
                  << ")";
        throw;
    }
    double ei_actual = person.risk_factors.at("EnergyIntake"_id);
    double pa_actual = person.risk_factors.at("PhysicalActivity"_id);
    double epa_actual = ei_actual / pa_actual;

    // Compute E/PA quantile.
    double epa_quantile = epa_actual / epa_expected;

    // Compute new weight.
    double w_expected =
        get_expected(context, person.gender, person.age, "Weight"_id, std::nullopt, true);
    const auto &weight_quantiles =
        resolve_weight_quantiles_for_person(person, weight_quantiles_by_stratum_);
    double w_quantile = get_weight_quantile(epa_quantile, weight_quantiles);
    person.risk_factors["Weight"_id] = w_expected * w_quantile;
    validate_weight_in_config_range(context, person, "initialise_weight");
}

void KevinHallModel::adjust_weight(const RuntimeContext &context, Person &person,
                                   double adjustment) const {
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
    validate_weight_in_config_range(context, person, "adjust_weight");
}

// MAHIMA: Added context and phase parameters to the validate_weight_in_config_range method
//  to provide more detailed error messages and allow for more specific validation in different
//  contexts.
// The context parameter is used to access the context's mapping and time information,
//  while the phase parameter is used to provide a more specific message about the validation error.
void KevinHallModel::validate_weight_in_config_range(const RuntimeContext &context,
                                                     const Person &person,
                                                     std::string_view phase) const {
    const auto weight_key = "Weight"_id;
    if (!person.risk_factors.contains(weight_key)) {
        return;
    }

    const double weight = person.risk_factors.at(weight_key);
    hgps::OptionalInterval range;
    try {
        range = context.mapping().at(weight_key).range();
    } catch (const std::out_of_range &) {
        return;
    }
    if (!range.has_value()) {
        return;
    }

    if (weight >= range->lower() && weight <= range->upper()) {
        return;
    }

    const bool above_maximum = weight > range->upper();
    const auto message = format_weight_range_violation_message(context, person, phase, weight,
                                                               *range, above_maximum);

    if (above_maximum) {
        std::osyncstream(std::cout) << "\n[WEIGHT RANGE WARNING] " << message << '\n';
        return;
    }

    throw core::HgpsException(message);
}

double KevinHallModel::get_weight_quantile(double epa_quantile,
                                           const std::vector<double> &quantiles) const {
    if (quantiles.empty()) {
        throw core::HgpsException("Weight quantile curve is empty");
    }

    // Compute Energy Physical Activity percentile (taking midpoint of duplicates).
    auto epa_range = std::equal_range(epa_quantiles_.begin(), epa_quantiles_.end(), epa_quantile);
    auto epa_index = static_cast<double>(std::distance(epa_quantiles_.begin(), epa_range.first));
    epa_index += std::distance(epa_range.first, epa_range.second) / 2.0;
    auto epa_percentile = epa_index / epa_quantiles_.size();

    // Find weight quantile.
    const size_t weight_index_last = quantiles.size() - 1;
    const auto weight_index = static_cast<size_t>(epa_percentile * weight_index_last);
    return quantiles[weight_index];
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
// MAHIMA: This function computes the mean weight for height for each person in the population.
//  It uses the height model parameters to compute the mean weight for height for each person.
//  It returns a table of mean weight for height by sex and age.
//  The height model parameters are stored in the height_params_ member variable.
//  The height model parameters are indexed by gender and income adjustment stratum.
//  The height model parameters are stored in the height_params_ member variable.
KevinHallAdjustmentTable
KevinHallModel::compute_mean_weight_for_height(Population &population,
                                               std::optional<unsigned> age) const {

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

    auto sumcounts = UnorderedMap2d<core::Gender, int, SumCount>{};
    sumcounts.emplace_row(core::Gender::female, std::unordered_map<int, SumCount>{});
    sumcounts.emplace_row(core::Gender::male, std::unordered_map<int, SumCount>{});
    for (const auto &person : population) {
        if (!person.is_active()) {
            continue;
        }
        if (age.has_value() && person.age != age.value()) {
            continue;
        }

        const double slope = resolve_height_params_for_person(person, height_params_).slope;
        const double value = pow(person.risk_factors.at("Weight"_id), slope);
        sumcounts.at(person.gender)[person.age].append(value);
    }

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

    const auto &height_params = resolve_height_params_for_person(person, height_params_);

    // Initialise lifelong height residual.
    double H_residual = random.next_normal(0, height_params.stddev);
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
    const auto &height_params = resolve_height_params_for_person(person, height_params_);
    const double stddev = height_params.stddev;
    const double slope = height_params.slope;

    // Compute height.
    double exp_norm_mean = exp(0.5 * stddev * stddev);
    double H = H_expected * (pow(W, slope) / W_power_mean) * (exp(H_residual) / exp_norm_mean);

    // Set height (may not exist yet).
    person.risk_factors["Height"_id] = H;
}

void KevinHallModel::print_weight_summary_tables(RuntimeContext &context,
                                                 std::string_view phase) const {
    if (!should_print_kevin_hall_summary_tables(context)) {
        return;
    }

    std::size_t bucket_count = 0;
    for (const auto &[gender, by_stratum] : weight_quantiles_by_stratum_) {
        (void)gender;
        bucket_count = std::max(bucket_count, by_stratum.size());
    }
    bucket_count = std::max<std::size_t>(bucket_count, 1);

    print_weight_stratum_assignment_table(context.population(), weight_quantiles_by_stratum_,
                                          bucket_count, context.time_now(), phase);

    const auto income_layout = hgps::core::income_category_layout_from_config(
        context.inputs().project_requirements().income.categories);
    print_weight_by_final_income_category_table(context.population(), income_layout,
                                                context.time_now(), phase);
}

void KevinHallModel::print_height_summary_tables(RuntimeContext &context,
                                                 std::string_view phase) const {
    if (!should_print_kevin_hall_summary_tables(context)) {
        return;
    }

    std::size_t bucket_count = 0;
    for (const auto &[gender, params] : height_params_) {
        bucket_count = std::max(bucket_count, params.size());
    }
    bucket_count = std::max<std::size_t>(bucket_count, 1);

    print_height_stratum_assignment_table(context.population(), height_params_, bucket_count,
                                          context.time_now(), phase);

    const auto income_layout = hgps::core::income_category_layout_from_config(
        context.inputs().project_requirements().income.categories);
    print_height_by_final_income_category_table(context.population(), income_layout,
                                                context.time_now(), phase);
}

KevinHallModelDefinition::KevinHallModelDefinition(
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    std::unordered_map<core::Identifier, double> energy_equation,
    std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges,
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
    std::unordered_map<core::Identifier, std::optional<double>> food_prices,
    std::unordered_map<core::Gender, std::vector<std::vector<double>>> weight_quantiles_by_stratum,
    std::vector<double> epa_quantiles,
    std::unordered_map<core::Gender, std::vector<HeightModelParams>> height_params)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps)},
      energy_equation_{std::move(energy_equation)}, nutrient_ranges_{std::move(nutrient_ranges)},
      nutrient_equations_{std::move(nutrient_equations)}, food_prices_{std::move(food_prices)},
      weight_quantiles_by_stratum_{std::move(weight_quantiles_by_stratum)},
      epa_quantiles_{std::move(epa_quantiles)}, height_params_{std::move(height_params)} {

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
    if (weight_quantiles_by_stratum_.empty()) {
        throw core::HgpsException("Weight quantiles mapping is empty");
    }
    for (const auto &[gender, by_stratum] : weight_quantiles_by_stratum_) {
        if (by_stratum.empty()) {
            throw core::HgpsException(
                fmt::format("Weight quantiles mapping contains empty stratum list for gender {}",
                            gender == core::Gender::male ? "male" : "female"));
        }
        for (const auto &quantiles : by_stratum) {
            if (quantiles.empty()) {
                throw core::HgpsException(
                    "Weight quantiles mapping contains an empty quantile curve");
            }
        }
    }
    if (epa_quantiles_.empty()) {
        throw core::HgpsException("Energy Physical Activity quantiles mapping is empty");
    }
    if (height_params_.empty()) {
        throw core::HgpsException("Height parameter mapping is empty");
    }
    for (const auto &[gender, params] : height_params_) {
        if (params.empty()) {
            throw core::HgpsException("Height parameter mapping contains empty gender entry");
        }
    }
}

std::unique_ptr<RiskFactorModel> KevinHallModelDefinition::create_model() const {
    return std::make_unique<KevinHallModel>(expected_, expected_trend_, trend_steps_,
                                            energy_equation_, nutrient_ranges_, nutrient_equations_,
                                            food_prices_, weight_quantiles_by_stratum_,
                                            epa_quantiles_, height_params_);
}

} // namespace hgps
// NOLINTEND(readability-convert-member-functions-to-static)
