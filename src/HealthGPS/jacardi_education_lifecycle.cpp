#include "jacardi_education_lifecycle.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/identifier.h"
#include "HealthGPS.Core/string_util.h"

#include <cmath>
#include <fmt/format.h>

namespace hgps {
namespace {
const auto education_factor = core::Identifier{"Education"};
} // namespace

void EducationLifecycleTables::set_lookup(std::unordered_map<std::uint64_t, Stratum> table) {
    lookup_ = std::move(table);
}

void EducationLifecycleTables::set_draw_at_22(std::unordered_map<std::uint64_t, Stratum> table) {
    draw_at_22_ = std::move(table);
}

void EducationLifecycleTables::set_upgrades(std::unordered_map<std::uint64_t, Stratum> table) {
    upgrades_ = std::move(table);
}

int EducationLifecycleTables::gender_code(core::Gender gender) {
    if (gender == core::Gender::male) {
        return 1;
    }
    if (gender == core::Gender::female) {
        return 0;
    }
    throw core::HgpsException{"JACARDI education: unknown gender"};
}

std::uint64_t EducationLifecycleTables::key_age_gender(int age, int gender) noexcept {
    return (static_cast<std::uint64_t>(age) << 1U) | static_cast<std::uint64_t>(gender & 1);
}

std::uint64_t EducationLifecycleTables::key_year_gender(int year, int gender) noexcept {
    return (static_cast<std::uint64_t>(year) << 1U) | static_cast<std::uint64_t>(gender & 1);
}

std::uint64_t EducationLifecycleTables::key_upgrade(int year, int age, int gender,
                                                    int from_id) noexcept {
    // year:12 | age:8 | gender:1 | from_id:4  (plenty for JACARDI ranges)
    return (static_cast<std::uint64_t>(year) << 13U) |
           (static_cast<std::uint64_t>(age & 0xff) << 5U) |
           (static_cast<std::uint64_t>(gender & 1) << 4U) |
           static_cast<std::uint64_t>(from_id & 0xf);
}

int EducationLifecycleTables::draw(const Stratum &stratum, Random &rng) {
    if (stratum.ids.empty()) {
        throw core::HgpsException{"JACARDI education DRAW: empty stratum"};
    }

    const double u = rng.next_double(); // [0, 1)
    double running = 0.0;
    int last_positive = stratum.ids.back();

    for (std::size_t i = 0; i < stratum.ids.size(); ++i) {
        const double p = stratum.probabilities[i];
        if (p > 0.0) {
            last_positive = stratum.ids[i];
        }
        running += p;
        if (running > u) {
            return stratum.ids[i];
        }
    }
    return last_positive;
}

double EducationLifecycleTables::initialise(unsigned int age, core::Gender gender,
                                            Random &rng) const {
    if (age <= 5U) {
        return missing;
    }
    if (age <= 18U) {
        return 2.0;
    }
    if (age <= 21U) {
        return 3.0;
    }

    if (lookup_.empty()) {
        throw core::HgpsException{"JACARDI education Part A: lookup table not loaded"};
    }

    const int g = gender_code(gender);
    const int capped_age = static_cast<int>(age > max_lookup_age ? max_lookup_age : age);
    const auto it = lookup_.find(key_age_gender(capped_age, g));
    if (it == lookup_.end()) {
        throw core::HgpsException{
            fmt::format("JACARDI education lookup missing age={} gender={}", capped_age, g)};
    }
    return static_cast<double>(draw(it->second, rng));
}

double EducationLifecycleTables::update(unsigned int age, core::Gender gender, int year,
                                        double current, Random &rng) const {
    if (age <= 5U) {
        return current; // stays missing
    }
    if (age == 6U) {
        return 2.0;
    }
    if (age <= 18U) {
        return current;
    }
    if (age == 19U) {
        return 3.0;
    }
    if (age <= 21U) {
        return current;
    }

    const int sim_year = year > max_update_year ? max_update_year : year;
    const int g = gender_code(gender);

    if (age == 22U) {
        if (draw_at_22_.empty()) {
            throw core::HgpsException{"JACARDI education Part B: draw_at_22 table not loaded"};
        }
        const auto it = draw_at_22_.find(key_year_gender(sim_year, g));
        if (it == draw_at_22_.end()) {
            throw core::HgpsException{
                fmt::format("JACARDI education draw_at_22 missing year={} gender={}", sim_year, g)};
        }
        return static_cast<double>(draw(it->second, rng));
    }

    if (age <= 30U) {
        if (upgrades_.empty()) {
            throw core::HgpsException{"JACARDI education Part B: upgrade table not loaded"};
        }
        if (!std::isfinite(current)) {
            throw core::HgpsException{
                "JACARDI education upgrade: current education is missing at ages 23-30"};
        }
        const int from_id = static_cast<int>(current);
        const auto it = upgrades_.find(key_upgrade(sim_year, static_cast<int>(age), g, from_id));
        if (it == upgrades_.end()) {
            // No transition rows => stay put (never go down).
            return current;
        }
        const int to_id = draw(it->second, rng);
        return static_cast<double>(to_id < from_id ? from_id : to_id);
    }

    return current; // 31+: fixed for life
}

void set_person_education(Person &person, double education_id) {
    person.risk_factors[education_factor] = education_id;
}

double get_person_education(const Person &person) {
    const auto it = person.risk_factors.find(education_factor);
    if (it == person.risk_factors.end()) {
        return EducationLifecycleTables::missing;
    }
    return it->second;
}

bool person_has_education(const Person &person) {
    return person.risk_factors.contains(education_factor);
}

} // namespace hgps
