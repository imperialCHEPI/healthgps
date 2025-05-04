#include "population.h"
#include <execution>
#include <iostream>

namespace hgps {
Population::Population(const std::size_t size) : initial_size_{size}, people_(size) {}

std::size_t Population::size() const noexcept { return people_.size(); }

std::size_t Population::initial_size() const noexcept { return initial_size_; }

std::size_t Population::current_active_size() const noexcept {
    auto active_pop_size = std::count_if(std::execution::par, people_.cbegin(), people_.cend(),
                                         [](const auto &p) { return p.is_active(); });

    return active_pop_size;
}

Person &Population::operator[](std::size_t index) { return people_[index]; }

const Person &Population::operator[](std::size_t index) const { return people_[index]; }

Person &Population::at(std::size_t index) { return people_.at(index); }

const Person &Population::at(std::size_t index) const { return people_.at(index); }

void Population::add(Person person, unsigned int time) noexcept {
    const auto recycle = find_index_of_recyclables(time, 1);
    if (!recycle.empty()) {
        people_.at(recycle.at(0)) = std::move(person);
    } else {
        people_.emplace_back(std::move(person));
    }
}

void Population::add_newborn_babies(std::size_t number, core::Gender gender,
                                    unsigned int time) noexcept {
    // std::cout << "DEBUG: add_newborn_babies called with " << number << " babies, gender: " <<
    // (gender == core::Gender::male ? "Male" : "Female") << std::endl;

    auto recycle = find_index_of_recyclables(time, number);
    auto remaining = number;

    if (!recycle.empty()) {
        auto replacebles = std::min(number, recycle.size());
        for (auto index = std::size_t{0}; index < replacebles; index++) {
            // Replace only inactive individuals - should be safe because find_index_of_recyclables
            // only returns indices of inactive people
            people_.at(recycle.at(index)) = Person{gender};
            remaining--;
        }
    }

    // Add the remaining new babies
    for (auto i = std::size_t{0}; i < remaining; i++) {
        people_.emplace_back(gender);
    }

    // std::cout << "DEBUG: After add_newborn_babies, active population: " << current_active_size()
    // << " of " << people_.size() << std::endl;
}

std::vector<int> Population::find_index_of_recyclables(unsigned int time,
                                                       std::size_t top) const noexcept {
    auto indices = std::vector<int>{};
    indices.reserve(top);

    int checked = 0;
    int inactive_count = 0;

    for (auto index = 0; const auto &entity : people_) {
        checked++;
        if (!entity.is_active()) {
            inactive_count++;
            if (entity.time_of_death() < time && entity.time_of_migration() < time) {
                indices.emplace_back(index);
                if (top > 0 && indices.size() >= top) {
                    break;
                }
            }
        }
        index++;
    }
    return indices;
}
} // namespace hgps
