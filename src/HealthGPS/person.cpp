#include "person.h"

#include "HealthGPS.Core/exception.h"
#include <algorithm> // For std::transform and tolower

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
    {"Income"_id, [](const Person &p) { return p.get_income_continuous(); }},
    {"income_category"_id, [](const Person &p) { return p.income_to_value(); }},
    {"Region"_id, [](const Person &p) { return p.region_to_value(); }},
    {"Ethnicity"_id, [](const Person &p) { return p.ethnicity_to_value(); }},
    {"SES"_id, [](const Person &p) { return p.ses; }},
};
// Modified- Mahima
// Creating default constructors (that don't require arguments)
// Pointed out by Clang-Tidy hence removed
Person::Person() : id_{++newUID} {} // Only initialize id_, other members use class defaults

// Modified- Mahima
// Creating parameterized constructor (that require arguments)
// Removed the default constructor as it was not being used in the code
// Pointed out by Clang-Tidy
Person::Person(const core::Gender birth_gender) noexcept
    : id_{++newUID},          // Must be first
      gender{birth_gender} {} // Only initialize id_ and gender

std::size_t Person::id() const noexcept { return id_; }

bool Person::is_alive() const noexcept { return is_alive_; }

unsigned int Person::time_of_death() const noexcept { return time_of_death_; }

unsigned int Person::time_of_migration() const noexcept { return time_of_migration_; }

double Person::get_risk_factor_value(const core::Identifier &key) const {
    if (current_dispatcher.contains(key)) {
        // Static properties
        return current_dispatcher.at(key)(*this);
    }

    // Check if the key matches any ethnicity name (case-insensitive)
    std::string key_str = key.to_string();
    std::string key_lower = key_str;
    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Handle ethnicity lookups by name
    if (key_lower == "white" && ethnicity == core::Ethnicity::White) {
        return ethnicity_to_value();
    } else if (key_lower == "asian" && ethnicity == core::Ethnicity::Asian) {
        return ethnicity_to_value();
    } else if (key_lower == "black" && ethnicity == core::Ethnicity::Black) {
        return ethnicity_to_value();
    } else if (key_lower == "others" && ethnicity == core::Ethnicity::Others) {
        return ethnicity_to_value();
    }

    // Also check if key matches the person's current ethnicity (by name)
    std::string person_ethnicity = ethnicity_to_string();
    std::string person_ethnicity_lower = person_ethnicity;
    std::transform(person_ethnicity_lower.begin(), person_ethnicity_lower.end(),
                   person_ethnicity_lower.begin(), [](unsigned char c) { return std::tolower(c); });

    if (key_lower == person_ethnicity_lower) {
        return ethnicity_to_value();
    }

    if (risk_factors.contains(key)) {
        // Dynamic properties
        return risk_factors.at(key);
    }

    // Try a case-insensitive search through risk factors
    for (const auto &[factor_key, factor_value] : risk_factors) {
        std::string factor_key_str = factor_key.to_string();
        std::transform(factor_key_str.begin(), factor_key_str.end(), factor_key_str.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (factor_key_str == key_lower) {
            return factor_value;
        }
    }

    throw std::out_of_range("Risk factor not found: " + key.to_string());
}

float Person::gender_to_value() const { return gender_to_value(gender); }

float Person::gender_to_value(core::Gender gender) {
    if (gender == core::Gender::unknown) {
        throw core::HgpsException("Gender is unknown.");
    }
    return gender == core::Gender::female ? 1.0f : 0.0f;
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
    switch (income_category) {
    case core::Income::low:
        return 1.0f;
    case core::Income::lowermiddle:
        return 2.0f;
    case core::Income::uppermiddle:
        return 3.0f;
    case core::Income::high:
        return 4.0f;
    default:
        throw core::HgpsException("Income category is unknown");
    }
}

double Person::get_income_continuous() const { return income_continuous; }

float Person::region_to_value() const {
    switch (region) {
    case core::Region::England:
        return 1.0f;
    case core::Region::Wales:
        return 2.0f;
    case core::Region::Scotland:
        return 3.0f;
    case core::Region::NorthernIreland:
        return 4.0f;
    default:
        throw core::HgpsException("Region is unknown.");
    }
}

std::string Person::region_to_string() const {
    switch (region) {
    case core::Region::England:
        return "England";
    case core::Region::Wales:
        return "Wales";
    case core::Region::Scotland:
        return "Scotland";
    case core::Region::NorthernIreland:
        return "Northern Ireland";
    default:
        throw core::HgpsException("Region is unknown.");
    }
}

float Person::ethnicity_to_value() const {
    switch (ethnicity) {
    case core::Ethnicity::White:
        return 1.0f;
    case core::Ethnicity::Asian:
        return 2.0f;
    case core::Ethnicity::Black:
        return 3.0f;
    case core::Ethnicity::Others:
        return 4.0f;
    case core::Ethnicity::unknown:
    default:
        throw core::HgpsException("Ethnicity is unknown.");
    }
}

std::string Person::ethnicity_to_string() const {
    switch (ethnicity) {
    case core::Ethnicity::White:
        return "White";
    case core::Ethnicity::Asian:
        return "Asian";
    case core::Ethnicity::Black:
        return "Black";
    case core::Ethnicity::Others:
        return "Others";
    default:
        throw core::HgpsException("Ethnicity is unknown.");
    }
}

std::string Person::income_category_to_string() const {
    switch (income_category) {
    case core::Income::low:
        return "Low";
    case core::Income::lowermiddle:
        return "Lower Middle";
    case core::Income::uppermiddle:
        return "Upper Middle";
    case core::Income::high:
        return "High";
    case core::Income::Continuous:
        return "Continuous";
    default:
        throw core::HgpsException("Income category is unknown.");
    }
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

bool Person::reactivate() {
    // Only reactivate if the person was incorrectly marked as inactive
    // but doesn't have a recorded death time or migration time
    if (!is_active() && time_of_death_ == 0 && time_of_migration_ == 0) {
        is_alive_ = true;
        has_emigrated_ = false;
        return true;
    }
    return false;
}

void Person::reset_id() { Person::newUID = 0; }

// Copy constructor because of unordered_map in Person class hence deep copy is needed
void Person::copy_from(const Person &other) {
    // Copy demographic properties
    age = other.age;
    gender = other.gender;
    region = other.region;
    ethnicity = other.ethnicity;
    income_continuous = other.income_continuous;
    income_category = other.income_category;
    sector = other.sector;
    ses = other.ses;

    // Copy risk factors and diseases
    risk_factors = other.risk_factors;
    diseases = other.diseases;

    // Do not copy identity, life status or migration status
    // The id_ remains unchanged
    // is_alive_, has_emigrated_, time_of_death_, time_of_migration_ remain unchanged
}

void Person::clear_diseases() { diseases.clear(); }

} // namespace hgps
