#include "pch.h"

#include "HealthGPS/jacardi_education_lifecycle.h"
#include "HealthGPS/random_algorithm.h"

#include "gtest/gtest.h"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

using namespace hgps;

namespace {

EducationLifecycleTables::Stratum make_stratum(std::vector<int> ids,
                                               std::vector<double> probs) {
    return EducationLifecycleTables::Stratum{std::move(ids), std::move(probs)};
}

} // namespace

TEST(JacardiEducation, InitialiseAgeBands) {
    EducationLifecycleTables tables;
    // Lookup required only for ages 22+; provide a trivial one.
    std::unordered_map<std::uint64_t, EducationLifecycleTables::Stratum> lookup;
    lookup.emplace(EducationLifecycleTables::key_age_gender(40, 1),
                   make_stratum({3}, {1.0}));
    tables.set_lookup(std::move(lookup));

    Random rng;
    rng.seed(1);
    EXPECT_TRUE(std::isnan(tables.initialise(0, core::Gender::male, rng)));
    EXPECT_DOUBLE_EQ(2.0, tables.initialise(10, core::Gender::female, rng));
    EXPECT_DOUBLE_EQ(3.0, tables.initialise(20, core::Gender::male, rng));
    EXPECT_DOUBLE_EQ(3.0, tables.initialise(40, core::Gender::male, rng));
}

TEST(JacardiEducation, DrawNeverGoesBelowFromIdOnUpgrade) {
    EducationLifecycleTables tables;
    std::unordered_map<std::uint64_t, EducationLifecycleTables::Stratum> upgrades;
    // from_id=3 can go to 3 or 5
    upgrades.emplace(EducationLifecycleTables::key_upgrade(2030, 25, 0, 3),
                     make_stratum({3, 5}, {0.5, 0.5}));
    tables.set_draw_at_22(
        {{EducationLifecycleTables::key_year_gender(2030, 0), make_stratum({3}, {1.0})}});
    tables.set_upgrades(std::move(upgrades));

    Random rng;
    rng.seed(42);
    for (int i = 0; i < 50; ++i) {
        const double next =
            tables.update(25, core::Gender::female, 2030, 3.0, rng);
        EXPECT_GE(next, 3.0);
    }
}

TEST(JacardiEducation, DrawUsesProbabilityMass) {
    Random rng;
    rng.seed(7);
    const auto stratum = make_stratum({1, 2}, {0.0, 1.0});
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(2, EducationLifecycleTables::draw(stratum, rng));
    }
}
