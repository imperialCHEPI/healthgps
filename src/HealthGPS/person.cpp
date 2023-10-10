#include "person.h"

namespace hgps {

std::atomic<std::size_t> Person::newUID{0};

std::map<core::Identifier, std::function<double(const Person &)>> Person::current_dispatcher{
    {core::Identifier{"Intercept"}, [](const Person &) { return 1.0; }},
    {core::Identifier{"Gender"}, [](const Person &p) { return p.gender_to_value(); }},
    {core::Identifier{"Age"}, [](const Person &p) { return static_cast<double>(p.age); }},
    {core::Identifier{"Age2"}, [](const Person &p) { return pow(p.age, 2); }},
    {core::Identifier{"Age3"}, [](const Person &p) { return pow(p.age, 3); }},
    {core::Identifier{"SES"}, [](const Person &p) { return p.ses; }},

    // HACK: ew, gross... allows us to mock risk factors we don't have data for yet
    {core::Identifier{"Height"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"Weight"}, [](const Person &) { return 0.5; }},
    //{core::Identifier{"BMI"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"PhysicalActivityLevel"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"BodyFat"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"LeanTissue"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"ExtracellularFluid"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"Glycogen"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"Water"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"EnergyExpenditure"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"EnergyIntake"}, [](const Person &) { return 0.5; }},
    {core::Identifier{"Carbohydrate"}, [](const Person &) { return 0.5; }},
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

float Person::gender_to_value() const noexcept {
    return gender == core::Gender::male ? 1.0f : 0.0f;
}

std::string Person::gender_to_string() const noexcept {
    return gender == core::Gender::male ? "male" : "female";
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
