#include "population.h"
#include <execution>

namespace hgps {

// MAHIMA: IDs are assigned from slot index (id = index + 1) so the same logical person has the
// same ID in baseline and intervention runs; Population is the only place that assigns these IDs.

Population::Population(const std::size_t size) : initial_size_{size} {
    // MAHIMA: Create each slot with ID = index + 1 for cross-scenario same-person tracking.
    people_.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        people_.emplace_back(i + 1);
    }
}

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
        const auto slot_index = static_cast<std::size_t>(recycle.at(0));
        people_.at(slot_index) = std::move(person);
        // MAHIMA: Assign slot-based ID so this person is tracked with same ID across scenarios.
        people_.at(slot_index).set_id(slot_index + 1);
    } else {
        people_.emplace_back(std::move(person));
        // MAHIMA: New slot gets ID = current size (index + 1).
        people_.back().set_id(people_.size());
    }
}

void Population::add_newborn_babies(std::size_t number, core::Gender gender,
                                    unsigned int time) noexcept {
    auto recycle = find_index_of_recyclables(time, number);
    auto remaining = number;
    if (!recycle.empty()) {
        auto replacebles = std::min(number, recycle.size());
        for (auto index = std::size_t{0}; index < replacebles; index++) {
            const auto slot_index = static_cast<std::size_t>(recycle.at(index));
            // MAHIMA: Newborn in recycled slot keeps that slot's ID (index + 1).
            people_.at(slot_index) = Person{gender, slot_index + 1};
            remaining--;
        }
    }

    for (auto i = std::size_t{0}; i < remaining; i++) {
        // MAHIMA: New slot gets ID = current size + 1 (next index + 1).
        people_.emplace_back(gender, people_.size() + 1);
    }
}

std::vector<int> Population::find_index_of_recyclables(unsigned int time,
                                                       std::size_t top) const noexcept {
    auto indices = std::vector<int>{};
    indices.reserve(top);
    for (auto index = 0; const auto &entity : people_) {
        if (!entity.is_active() && entity.time_of_death() < time &&
            entity.time_of_migration() < time) {
            indices.emplace_back(index);
            if (top > 0 && indices.size() >= top) {
                break;
            }
        }

        index++;
    }

    return indices;
}
} // namespace hgps
