#include "person.h"

namespace hgps {

    std::atomic<std::size_t> Person::newUID{0};
    
    Person::Person() 
        : id_{++Person::newUID}
    {}

    const std::size_t Person::id() const noexcept { return id_; }

    bool Person::is_active() const noexcept {
        return is_alive;
    }

    void Person::reset_id() {
        Person::newUID = 0;
    }
}
