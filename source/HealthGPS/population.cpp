#include "population.h"

namespace hgps {
    Population::Population(const std::size_t size) 
        : people_(size)
    {}

    std::size_t Population::size() const noexcept {
        return people_.size();
    }

    Person& Population::operator[](std::size_t index) {
        return people_[index];
    }

    const Person& Population::operator[](std::size_t index) const {
        return people_[index];
    }

    void Population::add(Person&& entity) noexcept {
        people_.push_back(entity);
    }

    void Population::add_newborn_babies(const int number, core::Gender gender) noexcept {
        for (size_t i = 0; i < number; i++) {
            people_.push_back(Person{ gender });
        }
    }
}