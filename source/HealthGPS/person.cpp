#include "person.h"

namespace hgps {

    std::atomic<std::size_t> Person::newUID{0};

    case_insensitive_map<std::function<double(const Person&)>> Person::dispatcher {
        {"intercept",[](const Person& p) { return 1.0; } },
        {"gender",[](const Person& p) { return p.gender_to_value(); } },
        {"age",[](const Person& p) { return static_cast<double>(p.age); } },
        {"age2",[](const Person& p) { return pow(p.age, 2); } },
        {"age3",[](const Person& p) { return pow(p.age, 3); } },
        {"education",[](const Person& p) { return static_cast<double>(p.education); } },
        {"income",[](const Person& p) { return static_cast<double>(p.income); } },
    };
    
    Person::Person() 
        : id_{++Person::newUID}
    {}

    const std::size_t Person::id() const noexcept { return id_; }

    bool Person::is_active() const noexcept {
        return is_alive;
    }

    double Person::get_risk_factor_value(const std::string& key) const noexcept {
        if (dispatcher.contains(key)) {
            return dispatcher.at(key)(*this);
        }

        if (risk_factors.contains(key)) {
            return risk_factors.at(key);
        }

        return std::nan("");
    }

    double Person::gender_to_value() const noexcept {
        if (gender == core::Gender::male) {
            return 1.0;
        }

        return 0.0;
    }

    void Person::reset_id() {
        Person::newUID = 0;
    }
}
