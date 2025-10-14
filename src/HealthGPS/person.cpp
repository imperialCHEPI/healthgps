#include "person.h"

#include "HealthGPS.Core/exception.h"

namespace hgps {

std::atomic<std::size_t> Person::newUID{0};

std::map<core::Identifier, std::function<double(const Person &)>> Person::current_dispatcher{
    {"Intercept"_id, [](const Person &) { return 1.0; }},
    {"Gender"_id, [](const Person &p) { return p.gender_to_value(); }},
    {"Age"_id, [](const Person &p) { return static_cast<double>(p.age); }},
    {"Age2"_id, [](const Person &p) { return pow(p.age, 2); }},
    {"Age3"_id, [](const Person &p) { return pow(p.age, 3); }},
    {"Over18"_id, [](const Person &p) { return static_cast<double>(p.over_18()); }},
    {"Sector"_id, [](const Person &p) { return p.sector_to_value(); }},
    {"Region"_id, [](const Person &p) { return p.region_to_value(); }},
    {"Ethnicity"_id, [](const Person &p) { return p.ethnicity_to_value(); }},
    {"Income"_id, [](const Person &p) { return p.income_to_value(); }},
    {"SES"_id, [](const Person &p) { return p.ses; }},
};

Person::Person() : id_{++Person::newUID} {}

Person::Person(const core::Gender birth_gender) noexcept
    : gender{birth_gender}, id_{++Person::newUID} {}

std::size_t Person::id() const noexcept { return id_; }

bool Person::is_alive() const noexcept { return is_alive_; }

bool Person::has_emigrated() const noexcept { return has_emigrated_; }

unsigned int Person::time_of_death() const noexcept { return time_of_death_; }

unsigned int Person::time_of_migration() const noexcept { return time_of_migration_; }

bool Person::is_active() const noexcept { return is_alive_ && !has_emigrated_; }

double Person::get_risk_factor_value(const core::Identifier &key) const {
    if (current_dispatcher.contains(key)) {
        // Static properties
        return current_dispatcher.at(key)(*this);
    }
    if (risk_factors.contains(key)) {
        // Dynamic properties
        return risk_factors.at(key);
    }
    throw std::out_of_range("Risk factor not found: " + key.to_string());
}

float Person::gender_to_value() const { return gender_to_value(gender); }

float Person::gender_to_value(core::Gender gender) {
    if (gender == core::Gender::unknown) {
        throw core::HgpsException("Gender is unknown.");
    }
    return gender == core::Gender::male ? 1.0f : 0.0f;
}

std::string Person::gender_to_string() const {
    if (gender == core::Gender::unknown) {
        throw core::HgpsException("Gender is unknown.");
    }
    return gender == core::Gender::male ? "male" : "female";
}

bool Person::over_18() const noexcept { return age >= 18; }

float Person::sector_to_value() const {
    switch (sector) {
    case core::Sector::urban:
        return 0.0f;
    case core::Sector::rural:
        return 1.0f;
    default:
        throw core::HgpsException("Sector is unknown.");
    }
}

float Person::income_to_value() const {
    switch (income) {
    case core::Income::low:
        return 1.0f; // Low income
    case core::Income::lowermiddle:
    case core::Income::middle:
        return 2.0f; // Both middle income categories map to same value for consistency
    case core::Income::uppermiddle:
        return 3.0f; // Upper middle income
    case core::Income::high:
        return 4.0f; // High income
    case core::Income::unknown:
    default:
        throw core::HgpsException("Unknown income category");
    }
}

float Person::region_to_value() const {
    // This method will need to be updated to work with the dynamic region mapping
    // For now, return a default value - the actual mapping will be implemented
    // when we implement the CSV loading system
    if (region == "unknown") {
        throw core::HgpsException(
            "Region is unknown - CSV data may not have been loaded properly.");
    }

    // TODO: Implement dynamic mapping based on CSV column order
    // This should be updated when we implement the CSV loading system
    // For now, return a hash-based value to ensure uniqueness
    std::hash<std::string> hasher;
    return static_cast<float>(hasher(region) % 1000) + 1.0f; // Ensure positive values
}

float Person::ethnicity_to_value() const {
    // This method will need to be updated to work with the dynamic ethnicity mapping
    // For now, return a default value - the actual mapping will be implemented
    // when we implement the CSV loading system
    if (ethnicity == "unknown") {
        throw core::HgpsException(
            "Ethnicity is unknown - CSV data may not have been loaded properly.");
    }

    // TODO: Implement dynamic mapping based on CSV column order
    // This should be updated when we implement the CSV loading system
    // For now, return a hash-based value to ensure uniqueness
    std::hash<std::string> hasher;
    return static_cast<float>(hasher(ethnicity) % 1000) + 1.0f; // Ensure positive values
}

void Person::emigrate(const unsigned int time) {
    if (!is_active()) {
        throw std::logic_error("Entity must be active prior to emigrate.");
    }

    has_emigrated_ = true;
    time_of_migration_ = time;
}

void Person::die(const unsigned int time) {
    if (!is_active()) {
        throw std::logic_error("Entity must be active prior to death.");
    }

    is_alive_ = false;
    time_of_death_ = time;
}

void Person::reset_id() { Person::newUID = 0; }
} // namespace hgps
