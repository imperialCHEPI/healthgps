#include "person.h"

namespace hgps {

    std::atomic<std::size_t> Person::newUID{0};

    case_insensitive_map<std::function<double(const Person&)>> Person::current_dispatcher {
        {"intercept",[](const Person& p) { return 1.0; } },
        {"gender",[](const Person& p) { return p.gender_to_value(); } },
        {"age",[](const Person& p) { return static_cast<double>(p.age); } },
        {"age2",[](const Person& p) { return pow(p.age, 2); } },
        {"age3",[](const Person& p) { return pow(p.age, 3); } },
        {"education",[](const Person& p) { return static_cast<double>(p.education.value()); } },
        {"income",[](const Person& p) { return static_cast<double>(p.income.value()); } },
    };

    case_insensitive_map<std::function<double(const Person&)>> Person::previous_dispatcher{
        {"intercept",[](const Person& p) { return 1.0; } },
        {"gender",[](const Person& p) { return p.gender_to_value(); } },
        {"age",[](const Person& p) { return static_cast<double>(p.age - 1); } },
        {"age2",[](const Person& p) { return pow(p.age - 1, 2); } },
        {"age3",[](const Person& p) { return pow(p.age - 1, 3); } },
        {"education",[](const Person& p) { return static_cast<double>(p.education.old_value()); } },
        {"income",[](const Person& p) { return static_cast<double>(p.income.old_value()); } },
    };
    
    Person::Person() 
        : id_{++Person::newUID}
    {}

    Person::Person(const core::Gender birth_gender) noexcept 
        : id_{ ++Person::newUID }, age{ 0 }, gender{ birth_gender }
    {}

    const std::size_t Person::id() const noexcept { return id_; }

    bool Person::is_active() const noexcept {
        return is_alive;
    }

    double Person::get_risk_factor_value(const std::string& key) const noexcept {
        if (current_dispatcher.contains(key)) {
            return current_dispatcher.at(key)(*this);
        }

        if (risk_factors.contains(key)) {
            return risk_factors.at(key);
        }

        return std::nan("");
    }

    double Person::get_previous_risk_factor_value(const std::string& key) const noexcept {
        if (previous_dispatcher.contains(key)) {
            return previous_dispatcher.at(key)(*this);
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

    void Person::reset_id() {
        Person::newUID = 0;
    }
}
