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
    {"Income"_id, [](const Person &p) { return p.income_to_value(); }},
    {"Region"_id, [](const Person &p) { return p.region_to_value(); }},
    {"Ethnicity"_id, [](const Person &p) { return p.ethnicity_to_value(); }},
    {"SES"_id, [](const Person &p) { return p.ses; }},
};

Person::Person()
    : age{0}, gender{core::Gender::unknown}, // Changed from female to unknown
      region{core::Region::unknown}, ethnicity{core::Ethnicity::unknown},
      sector{core::Sector::urban}, income_continuous{0.0}, income_category{core::Income::low},
      ses{0.0}, is_alive_{true}, has_emigrated_{false}, time_of_death_{0}, time_of_migration_{0},
      id_{++Person::newUID} {}

Person::Person(const core::Gender birth_gender) noexcept
    : age{0}, gender{birth_gender}, region{core::Region::unknown},
      ethnicity{core::Ethnicity::unknown}, sector{core::Sector::urban}, income_continuous{0.0},
      income_category{core::Income::low}, ses{0.0}, is_alive_{true}, has_emigrated_{false},
      time_of_death_{0}, time_of_migration_{0}, id_{++Person::newUID} {}

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
    case core::Region::unknown:
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

// Copy constructor because of unordered_map in Person class hence deep copy is needed
void Person::copy_from(const Person &other) {
    age = other.age;
    gender = other.gender;
    region = other.region;
    ethnicity = other.ethnicity;
    sector = other.sector;
    income_continuous = other.income_continuous;
    income_category = other.income_category;
    risk_factors = other.risk_factors;
}
} // namespace hgps
