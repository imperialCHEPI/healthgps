#include "person.h"

namespace hgps {

    std::atomic<std::size_t> Person::newUID{0};

    std::map<core::Identifier, std::function<double(const Person&)>> Person::current_dispatcher {
        {core::Identifier{"intercept"},[](const Person&) { return 1.0; } },
        {core::Identifier{"gender"},[](const Person& p) { return p.gender_to_value(); } },
        {core::Identifier{"age"},[](const Person& p) { return static_cast<double>(p.age); } },
        {core::Identifier{"age2"},[](const Person& p) { return pow(p.age, 2); } },
        {core::Identifier{"age3"},[](const Person& p) { return pow(p.age, 3); } },
        {core::Identifier{"ses"},[](const Person& p) { return p.ses; } },
    };
   
    Person::Person() 
        : id_{++Person::newUID}
    {}

    Person::Person(const core::Gender birth_gender) noexcept 
        : gender{ birth_gender }, age{ 0 }, id_{ ++Person::newUID }
    {}

    std::size_t Person::id() const noexcept { return id_; }

    bool Person::is_alive() const noexcept {
        return is_alive_;
    }

    bool Person::has_emigrated() const noexcept {
        return has_emigrated_;
    }

    unsigned int Person::time_of_death() const noexcept {
        return time_of_death_;
    }

    unsigned int Person::time_of_migration() const noexcept {
        return time_of_migration_;
    }

    bool Person::is_active() const noexcept {
        return is_alive_ && !has_emigrated_;
    }

    double Person::get_risk_factor_value(const core::Identifier& key) const noexcept {
        if (current_dispatcher.contains(key)) {
            return current_dispatcher.at(key)(*this);
        }

        if (risk_factors.contains(key)) {
            return risk_factors.at(key);
        }

        return std::nan("");
    }

    float Person::gender_to_value() const noexcept {
        if (gender == core::Gender::male) {
            return 1.0f;
        }

        return 0.0f;
    }

    std::string Person::gender_to_string() const noexcept
    {
        if (gender == core::Gender::male) {
            return "male";
        }

        return "female";
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

    void Person::reset_id() {
        Person::newUID = 0;
    }
}
