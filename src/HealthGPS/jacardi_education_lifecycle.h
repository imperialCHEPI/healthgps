#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "person.h"
#include "random_algorithm.h"

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hgps {

/// MAHIMA: Slovenia-style education lifecycle (Part A init / Part B yearly).
/// education_id is 1..7, or missing (NaN on Person). CSV gender: 1=male, 0=female.
class EducationLifecycleTables {
  public:
    static constexpr double missing = std::numeric_limits<double>::quiet_NaN();
    static constexpr int max_lookup_age = 110;
    static constexpr int max_update_year = 2110;

    /// One empirical stratum: ids sorted ascending with matching probabilities.
    struct Stratum {
        std::vector<int> ids;
        std::vector<double> probabilities;
    };

    void set_lookup(std::unordered_map<std::uint64_t, Stratum> table);
    void set_draw_at_22(std::unordered_map<std::uint64_t, Stratum> table);
    void set_upgrades(std::unordered_map<std::uint64_t, Stratum> table);

    bool has_lookup() const noexcept { return !lookup_.empty(); }
    bool has_draw_at_22() const noexcept { return !draw_at_22_.empty(); }
    bool has_upgrades() const noexcept { return !upgrades_.empty(); }

    /// Part A — start-year assignment for one person.
    double initialise(unsigned int age, core::Gender gender, Random &rng) const;

    /// Part B — yearly update after ageing. `current` may be missing (NaN).
    double update(unsigned int age, core::Gender gender, int year, double current,
                  Random &rng) const;

    static int gender_code(core::Gender gender);
    static std::uint64_t key_age_gender(int age, int gender) noexcept;
    static std::uint64_t key_year_gender(int year, int gender) noexcept;
    static std::uint64_t key_upgrade(int year, int age, int gender, int from_id) noexcept;

    /// DRAW: sorted ids + probs → one education_id (matches EDUCATION_ALGORITHM.md).
    static int draw(const Stratum &stratum, Random &rng);

  private:
    std::unordered_map<std::uint64_t, Stratum> lookup_;
    std::unordered_map<std::uint64_t, Stratum> draw_at_22_;
    std::unordered_map<std::uint64_t, Stratum> upgrades_;
};

/// MAHIMA: Read/write Person education risk factor ("Education").
void set_person_education(Person &person, double education_id);
double get_person_education(const Person &person);
bool person_has_education(const Person &person);

} // namespace hgps
