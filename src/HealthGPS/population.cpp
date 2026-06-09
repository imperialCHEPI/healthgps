#include "population.h"
#include <execution>

#ifndef NDEBUG
#include <cassert>
#endif

namespace hgps {

// MAHIMA: Initial cohort IDs are slot-aligned (id = index + 1) across baseline/intervention.
// Post-initial entrants use a monotonic Population counter so IDs are lifetime-unique and never
// reused when slots are recycled.

Population::Population(const std::size_t size) : initial_size_{size} {
    // MAHIMA: Initial cohort uses deterministic IDs for cross-scenario same-person alignment.
    people_.reserve(size);
#ifndef NDEBUG
    // MAHIMA: Initial IDs must be consecutive and unused before post-initial counter takes over.
    assert(next_person_id_ == 1u);
#endif
    for (std::size_t i = 0; i < size; ++i) {
        const std::size_t id = i + 1u;
#ifndef NDEBUG
        assert(id == next_person_id_);
        assert(id != Person::unassigned_id);
#endif
        people_.emplace_back(id);
        ++next_person_id_;
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
        // MAHIMA: Reused slot gets a fresh lifetime-unique ID (slot reuse != ID reuse).
        people_.at(slot_index).set_id(allocate_next_person_id());
    } else {
        people_.emplace_back(std::move(person));
        // MAHIMA: Appended entrant also gets a fresh lifetime-unique ID.
        people_.back().set_id(allocate_next_person_id());
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
            // MAHIMA: Newborn in recycled slot gets a fresh lifetime-unique ID.
            people_.at(slot_index) = Person{gender, allocate_next_person_id()};
            remaining--;
        }
    }

    for (auto i = std::size_t{0}; i < remaining; i++) {
        // MAHIMA: Newborn in appended slot also gets a fresh lifetime-unique ID.
        people_.emplace_back(gender, allocate_next_person_id());
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
