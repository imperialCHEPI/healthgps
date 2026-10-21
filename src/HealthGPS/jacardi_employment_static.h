#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "person.h"
#include "random_algorithm.h"

#include <array>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hgps {

/// MAHIMA: Slovenia employment static (partner algo 16 Oct 2026) — P1b after education.
///
/// Part A (`initialise`): start year + still-missing adults.
/// Part B (`update`): after ageing + education; first SCORE/DRAW at 19+ when missing;
/// otherwise temporary hold (swappable when yearly package arrives).
///
/// Person fields: `EmploymentDetailed` (1..6) and `Employment` (1..4). Ages 0–18 = NaN.
/// CSV gender: 1 = male, 0 = female. Needs finite education_id at ages 19+.
///
/// Serial person loops only — shared Random is not thread-safe (same as education).
class EmploymentStaticTables {
  public:
    static constexpr double missing = std::numeric_limits<double>::quiet_NaN();
    static constexpr unsigned int age_missing_max = 18U;
    static constexpr unsigned int age_assign_min = 19U;
    static constexpr int age_cap = 80;
    static constexpr int category_count = 6;
    static constexpr int category_min = 1;
    static constexpr int category_max = 6;

    struct FactorTerm {
        std::string factor;
        double coefficient{};
    };

    /// Drawn detailed id + recoded 4-cat id (both NaN when missing).
    struct Assignment {
        double detailed_id{missing};
        double cat4_id{missing};
    };

    /// Age-only predictors (precomputed for 19..80).
    struct AgeFactors {
        double age{};
        std::array<double, 5> age_rcs{};
        double school_fade{};
    };

    void set_coefficients(std::unordered_map<int, std::vector<FactorTerm>> by_category);
    void set_recode(std::array<int, category_count + 1> detailed_to_cat4);

    bool has_coefficients() const noexcept { return !coefficients_.empty(); }
    bool has_recode() const noexcept { return recode_loaded_; }

    /// Softmax probabilities p_1..p_6 for SCORE checks (indices 0..5 = categories 1..6).
    std::array<double, category_count> score_probabilities(unsigned int age, core::Gender gender,
                                                           int education_id) const;

    /// Part A — missing 0..18; else SCORE → DRAW → recode.
    Assignment initialise(unsigned int age, core::Gender gender, int education_id,
                          Random &rng) const;

    /// Part B — missing under 19; first assign if still missing at 19+; else hold (temporary).
    Assignment update(unsigned int age, core::Gender gender, int education_id,
                      const Assignment &current, Random &rng) const;

    /// MAHIMA: Isolated hold — yearly package replaces this path only.
    static Assignment hold(const Assignment &current) noexcept { return current; }

    int recode(int employment_id) const;

    static int gender_code(core::Gender gender);
    static AgeFactors compute_age_factors(int age_capped);
    static double factor_value(const std::string &factor, const AgeFactors &age_f, int gender,
                               int education_id);

  private:
    Assignment assign_draw(unsigned int age, core::Gender gender, int education_id,
                           Random &rng) const;
    static int draw_category(const std::array<double, category_count> &probs, Random &rng);

    std::unordered_map<int, std::vector<FactorTerm>> coefficients_;
    // MAHIMA: index = employment_id 1..6; [0] unused.
    std::array<int, category_count + 1> recode_{};
    bool recode_loaded_{false};
};

void set_person_employment(Person &person, const EmploymentStaticTables::Assignment &assignment);
EmploymentStaticTables::Assignment get_person_employment(const Person &person);
bool person_has_employment(const Person &person);

} // namespace hgps
