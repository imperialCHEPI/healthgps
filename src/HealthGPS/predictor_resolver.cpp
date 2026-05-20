#include "predictor_resolver.h"

#include "HealthGPS.Core/string_util.h"

#include <cctype>
#include <cmath>
#include <string>

namespace hgps {
namespace {

constexpr double k_log_floor = 1e-10;

bool parse_trailing_power(const std::string &name, const std::string &prefix, int &power_out) {
    if (!name.starts_with(prefix)) {
        return false;
    }
    if (name.size() == prefix.size()) {
        power_out = 1;
        return true;
    }
    const auto suffix = name.substr(prefix.size());
    if (suffix.empty() || !std::isdigit(static_cast<unsigned char>(suffix.front()))) {
        return false;
    }
    try {
        power_out = std::stoi(suffix);
        return true;
    } catch (...) {
        return false;
    }
}

double income_base_value(const Person &person) {
    const core::Identifier income_id("income");
    if (person.risk_factors.contains(income_id)) {
        return person.risk_factors.at(income_id);
    }
    if (person.income_continuous > 0.0) {
        return person.income_continuous;
    }
    return person.income_to_value();
}

double age_polynomial(const Person &person, int power) {
    const auto age = static_cast<double>(person.age);
    return std::pow(age, power);
}

double gender_dummy(const Person &person) {
    return person.gender == core::Gender::male ? 1.0 : 0.0;
}

double region_dummy(const Person &person, const std::string &factor_name) {
    if (factor_name == "region") {
        try {
            return std::stod(person.region);
        } catch (...) {
            return person.region == factor_name ? 1.0 : 0.0;
        }
    }
    return person.region == factor_name ? 1.0 : 0.0;
}

double ethnicity_dummy(const Person &person, const std::string &factor_name) {
    if (factor_name == "ethnicity") {
        try {
            return std::stod(person.ethnicity);
        } catch (...) {
            return person.ethnicity == factor_name ? 1.0 : 0.0;
        }
    }
    return person.ethnicity == factor_name ? 1.0 : 0.0;
}

std::optional<double> resolve_log_predictor(const Person &person, const std::string &key) {
    if (!key.starts_with("log_")) {
        return std::nullopt;
    }
    auto remainder = key.substr(4);
    if (remainder.empty()) {
        return std::nullopt;
    }

    // log_income2 -> (log(income))^2 : trailing digits are power on the log, not on the base.
    int log_power = 1;
    size_t base_end = remainder.size();
    while (base_end > 0 && std::isdigit(static_cast<unsigned char>(remainder[base_end - 1]))) {
        --base_end;
    }
    if (base_end < remainder.size()) {
        try {
            log_power = std::stoi(remainder.substr(base_end));
        } catch (...) {
            return std::nullopt;
        }
    }
    const auto base_name = remainder.substr(0, base_end);
    if (base_name.empty()) {
        return std::nullopt;
    }

    const auto base_value = resolve_derived_predictor(person, base_name);
    if (!base_value.has_value()) {
        return std::nullopt;
    }
    const double logged = std::log(std::max(base_value.value(), k_log_floor));
    return std::pow(logged, log_power);
}

std::optional<double> resolve_age_predictor(const Person &person, const std::string &key) {
    if (core::case_insensitive::equals(key, "age") || core::case_insensitive::equals(key, "Age")) {
        return age_polynomial(person, 1);
    }
    int power = 0;
    if (parse_trailing_power(key, "age", power) || parse_trailing_power(key, "Age", power)) {
        return age_polynomial(person, power);
    }
    return std::nullopt;
}

std::optional<double> resolve_income_predictor(const Person &person, const std::string &key) {
    if (key == "income_continuous") {
        return person.income_continuous;
    }
    int power = 0;
    if (parse_trailing_power(key, "income", power)) {
        return std::pow(income_base_value(person), power);
    }
    if (core::case_insensitive::equals(key, "income") ||
        core::case_insensitive::equals(key, "Income")) {
        return income_base_value(person);
    }
    return std::nullopt;
}

std::optional<double> resolve_gender_predictor(const Person &person, const std::string &key) {
    int power = 0;
    if (parse_trailing_power(key, "gender", power)) {
        if (power == 1 && key == "gender2") {
            return gender_dummy(person);
        }
        return std::pow(gender_dummy(person), power);
    }
    if (key == "gender2") {
        return gender_dummy(person);
    }
    if (key == "gender") {
        return person.gender_to_value();
    }
    return std::nullopt;
}

std::optional<double> resolve_sector_predictor(const Person &person, const std::string &key) {
    int power = 0;
    if (parse_trailing_power(key, "sector", power)) {
        return std::pow(person.sector_to_value(), power);
    }
    if (key == "sector") {
        return person.sector_to_value();
    }
    return std::nullopt;
}

std::optional<double> resolve_energyintake_predictor(const Person &person,
                                                     const std::string &key) {
    if (!core::case_insensitive::equals(key, "energyintake")) {
        return std::nullopt;
    }
    const core::Identifier energy_intake_id("EnergyIntake");
    const core::Identifier energyintake_lower("energyintake");
    if (person.risk_factors.contains(energyintake_lower)) {
        return person.risk_factors.at(energyintake_lower);
    }
    if (person.risk_factors.contains(energy_intake_id)) {
        return person.risk_factors.at(energy_intake_id);
    }
    return std::nullopt;
}

} // namespace

bool is_metadata_predictor(const std::string &name) {
    return core::case_insensitive::equals(name, "stddev") ||
           core::case_insensitive::equals(name, "min") ||
           core::case_insensitive::equals(name, "max") ||
           core::case_insensitive::equals(name, "lambda") ||
           core::case_insensitive::equals(name, "intercept");
}

bool is_metadata_predictor(const core::Identifier &name) {
    return is_metadata_predictor(name.to_string());
}

std::optional<double> resolve_derived_predictor(const Person &person, const std::string &key) {
    if (key.empty() || is_metadata_predictor(key)) {
        return std::nullopt;
    }
    if (auto log_value = resolve_log_predictor(person, key)) {
        return log_value;
    }
    if (auto income_value = resolve_income_predictor(person, key)) {
        return income_value;
    }
    if (key == person.region || key == person.ethnicity) {
        return 1.0;
    }
    if (auto age_value = resolve_age_predictor(person, key)) {
        return age_value;
    }
    if (auto gender_value = resolve_gender_predictor(person, key)) {
        return gender_value;
    }
    if (key.starts_with("region")) {
        return region_dummy(person, key);
    }
    if (key.starts_with("ethnicity")) {
        return ethnicity_dummy(person, key);
    }
    if (auto sector_value = resolve_sector_predictor(person, key)) {
        return sector_value;
    }
    return resolve_energyintake_predictor(person, key);
}

} // namespace hgps
