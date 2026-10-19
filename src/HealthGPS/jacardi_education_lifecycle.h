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

/// MAHIMA: Slovenia-style education lifecycle for JACARDI (partner algo 15 Oct 2026).
///
/// Two phases (same tables object, different call sites):
/// - Part A (`initialise`) — start year + newborns without a finite Education value.
/// - Part B (`update`) — every year after DemographicModule ages people.
///
/// Values live on `Person.risk_factors["Education"]` as education_id 1..7.
/// Ages 0–18 use NaN (= missing). CSV gender codes: 1 = male, 0 = female.
///
/// Threading: tables are immutable after load → safe for concurrent reads.
/// Do NOT parallelise the person loops while they share one `Random` — RNG is not
/// thread-safe. Per-person work is a map lookup + a few compares; analysis/disease
/// paths dominate runtime, so leave these loops serial until we have thread-local RNG.
///
/// Age thresholds are named constants below so partners can retune rules without
/// hunting magic numbers.
class EducationLifecycleTables {
  public:
    static constexpr double missing = std::numeric_limits<double>::quiet_NaN();
    static constexpr int max_lookup_age = 110;
    static constexpr int max_update_year = 2110;

    // MAHIMA: 15 Oct 2026 — missing through 18; first draw at 19; upgrades 20–30; fixed 31+.
    static constexpr unsigned int age_missing_max = 18U; // 0..18 → no education
    static constexpr unsigned int age_draw = 19U;        // Part B: draw from year×gender CSV
    static constexpr unsigned int age_upgrade_max = 30U; // Part B: 20..30 upgrade transitions

    /// One empirical stratum: sorted education ids + matching probabilities (sum ≈ 1).
    struct Stratum {
        std::vector<int> ids;
        std::vector<double> probabilities;
    };

    void set_lookup(std::unordered_map<std::uint64_t, Stratum> table);
    void set_draw_at_19(std::unordered_map<std::uint64_t, Stratum> table);
    void set_upgrades(std::unordered_map<std::uint64_t, Stratum> table);

    bool has_lookup() const noexcept { return !lookup_.empty(); }
    bool has_draw_at_19() const noexcept { return !draw_at_19_.empty(); }
    bool has_upgrades() const noexcept { return !upgrades_.empty(); }

    /// Part A — assign education once for a person at a given age.
    double initialise(unsigned int age, core::Gender gender, Random &rng) const;

    /// Part B — yearly step after ageing. `current` may be NaN (missing).
    double update(unsigned int age, core::Gender gender, int year, double current,
                  Random &rng) const;

    static int gender_code(core::Gender gender);
    static std::uint64_t key_age_gender(int age, int gender) noexcept;
    static std::uint64_t key_year_gender(int year, int gender) noexcept;
    static std::uint64_t key_upgrade(int year, int age, int gender, int from_id) noexcept;

    /// Inverse-CDF draw over a stratum (ids sorted ascending).
    static int draw(const Stratum &stratum, Random &rng);

  private:
    std::unordered_map<std::uint64_t, Stratum> lookup_;
    std::unordered_map<std::uint64_t, Stratum> draw_at_19_;
    std::unordered_map<std::uint64_t, Stratum> upgrades_;
};

/// MAHIMA: Person.risk_factors["Education"] helpers.
/// `person_has_education` is true only for a finite id (NaN / absent = missing).
void set_person_education(Person &person, double education_id);
double get_person_education(const Person &person);
bool person_has_education(const Person &person);

} // namespace hgps
