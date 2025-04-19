#include "person.h"

#include "HealthGPS.Core/exception.h"

namespace hgps {

std::atomic<std::size_t> Person::newUID{0};

std::unordered_map<core::Identifier, std::function<double(const Person &)>>
    Person::current_dispatcher{
        {"Intercept"_id, [](const Person &) { return 1.0; }},
        {"Gender"_id, [](const Person &p) { return p.gender_to_value(); }},
        {"Age"_id, [](const Person &p) { return static_cast<double>(p.age); }},
        {"Age2"_id, [](const Person &p) { return pow(p.age, 2); }},
        {"Age3"_id, [](const Person &p) { return pow(p.age, 3); }},
        {"Over18"_id, [](const Person &p) { return static_cast<double>(p.over_18()); }},
        {"Sector"_id, [](const Person &p) { return p.sector_to_value(); }},
        {"Income"_id, [](const Person &p) { return p.income_to_value(); }},
        {"SES"_id, [](const Person &p) { return p.ses; }},
        {"Region"_id, [](const Person &p) { return p.region_to_value(); }},
        {"Ethnicity"_id, [](const Person &p) { return p.ethnicity_to_value(); }},
        {"PhysicalActivity"_id, [](const Person &p) { return p.physical_activity; }},
        {"IncomeContinuous"_id, [](const Person &p) { return p.income_continuous; }},

        // Add handlers for specific ethnicity coefficients
        {"White"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::White ? 1.0 : 0.0; }},
        {"white"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::White ? 1.0 : 0.0; }},
        {"Black"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Black ? 1.0 : 0.0; }},
        {"black"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Black ? 1.0 : 0.0; }},
        {"Asian"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Asian ? 1.0 : 0.0; }},
        {"asian"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Asian ? 1.0 : 0.0; }},
        {"Mixed"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Mixed ? 1.0 : 0.0; }},
        {"mixed"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Mixed ? 1.0 : 0.0; }},
        {"Other"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Other ? 1.0 : 0.0; }},
        {"other"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Other ? 1.0 : 0.0; }},
        {"Others"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Other ? 1.0 : 0.0; }},
        {"others"_id,
         [](const Person &p) { return p.ethnicity == core::Ethnicity::Other ? 1.0 : 0.0; }},

        // Add handlers for specific region coefficients if needed
        {"England"_id,
         [](const Person &p) { return p.region == core::Region::England ? 1.0 : 0.0; }},
        {"Wales"_id, [](const Person &p) { return p.region == core::Region::Wales ? 1.0 : 0.0; }},
        {"Scotland"_id,
         [](const Person &p) { return p.region == core::Region::Scotland ? 1.0 : 0.0; }},
        {"NorthernIreland"_id,
         [](const Person &p) { return p.region == core::Region::NorthernIreland ? 1.0 : 0.0; }},
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
        // Return default value (urban) instead of throwing an exception
        return 0.0f; // Default to urban (0.0) if sector is unknown
    }
}

float Person::income_to_value() const {
    switch (income) {
    case core::Income::low:
        return 1.0f;
    case core::Income::lowermiddle:
        return 2.0f;
    case core::Income::uppermiddle:
        return 3.0f;
    case core::Income::high:
        return 4.0f;
    case core::Income::unknown:
    default:
        throw core::HgpsException("Unknown income category");
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
    case core::Ethnicity::Black:
        return 2.0f;
    case core::Ethnicity::Asian:
        return 3.0f;
    case core::Ethnicity::Mixed:
        return 4.0f;
    case core::Ethnicity::Other:
        return 5.0f;
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

// Copy demographics from another Person instance
void Person::copy_from(const Person &other) {
    // Copy demographic attributes
    age = other.age;
    gender = other.gender;
    region = other.region;
    ethnicity = other.ethnicity;
    income_continuous = other.income_continuous;
    income = other.income;
    income_category = other.income_category;
    sector = other.sector;
    ses = other.ses;
    physical_activity = other.physical_activity;

    // Copy risk factors and diseases
    risk_factors = other.risk_factors;
    diseases = other.diseases;

    // Don't copy private members like is_alive_, has_emigrated_, etc.
}
} // namespace hgps
