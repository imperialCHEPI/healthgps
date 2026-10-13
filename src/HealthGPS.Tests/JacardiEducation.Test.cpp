#include "pch.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS/jacardi_education_lifecycle.h"
#include "HealthGPS/random_algorithm.h"

#include "gtest/gtest.h"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

using namespace hgps;

namespace {

EducationLifecycleTables::Stratum make_stratum(std::vector<int> ids, std::vector<double> probs) {
    return EducationLifecycleTables::Stratum{.ids = std::move(ids),
                                             .probabilities = std::move(probs)};
}

} // namespace

TEST(JacardiEducation, InitialiseAgeBands) {
    EducationLifecycleTables tables;
    // Lookup required only for ages 22+; provide a trivial one.
    std::unordered_map<std::uint64_t, EducationLifecycleTables::Stratum> lookup;
    lookup.emplace(EducationLifecycleTables::key_age_gender(40, 1), make_stratum({3}, {1.0}));
    tables.set_lookup(std::move(lookup));

    Random rng;
    rng.seed(1);
    EXPECT_TRUE(std::isnan(
        tables.initialise(EducationLifecycleTables::age_missing_max, core::Gender::male, rng)));
    EXPECT_DOUBLE_EQ(EducationLifecycleTables::primary_education_id,
                     tables.initialise(10, core::Gender::female, rng));
    EXPECT_DOUBLE_EQ(EducationLifecycleTables::secondary_education_id,
                     tables.initialise(20, core::Gender::male, rng));
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
        const double next = tables.update(25, core::Gender::female, 2030, 3.0, rng);
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

TEST(JacardiEducation, DrawEmptyStratumThrows) {
    Random rng;
    EXPECT_THROW(EducationLifecycleTables::draw(make_stratum({}, {}), rng), core::HgpsException);
}

TEST(JacardiEducation, DrawFallsBackWhenMassBelowOne) {
    Random rng;
    rng.seed(1);
    // Mass sums to 0.1; u often exceeds running total → last positive id.
    const auto stratum = make_stratum({1, 7}, {0.05, 0.05});
    for (int i = 0; i < 30; ++i) {
        const int id = EducationLifecycleTables::draw(stratum, rng);
        EXPECT_TRUE(id == 1 || id == 7);
    }
}

TEST(JacardiEducation, UnknownGenderThrows) {
    EXPECT_THROW(EducationLifecycleTables::gender_code(core::Gender::unknown), core::HgpsException);
}

TEST(JacardiEducation, InitialiseWithoutLookupThrows) {
    EducationLifecycleTables tables;
    Random rng;
    EXPECT_THROW(tables.initialise(40, core::Gender::male, rng), core::HgpsException);
}

TEST(JacardiEducation, InitialiseCapsAgeAt110) {
    EducationLifecycleTables tables;
    std::unordered_map<std::uint64_t, EducationLifecycleTables::Stratum> lookup;
    lookup.emplace(EducationLifecycleTables::key_age_gender(110, 1), make_stratum({6}, {1.0}));
    tables.set_lookup(std::move(lookup));
    Random rng;
    rng.seed(1);
    EXPECT_DOUBLE_EQ(6.0, tables.initialise(120, core::Gender::male, rng));
}

TEST(JacardiEducation, UpdateAgeMilestonesAndCaps) {
    EducationLifecycleTables tables;
    tables.set_draw_at_22(
        {{EducationLifecycleTables::key_year_gender(2110, 1), make_stratum({3}, {1.0})}});
    // Non-empty map but no row for (2026,25,male,3) → stay put.
    tables.set_upgrades(
        {{EducationLifecycleTables::key_upgrade(2026, 24, 1, 3), make_stratum({3}, {1.0})}});

    Random rng;
    rng.seed(1);
    EXPECT_TRUE(std::isnan(
        tables.update(3, core::Gender::male, 2026, EducationLifecycleTables::missing, rng)));
    EXPECT_DOUBLE_EQ(2.0, tables.update(6, core::Gender::male, 2026, 0.0, rng));
    EXPECT_DOUBLE_EQ(7.0, tables.update(10, core::Gender::male, 2026, 7.0, rng));
    EXPECT_DOUBLE_EQ(3.0, tables.update(19, core::Gender::male, 2026, 2.0, rng));
    EXPECT_DOUBLE_EQ(3.0, tables.update(20, core::Gender::male, 2026, 3.0, rng));
    // year 9999 capped to 2110 for draw_at_22 key
    EXPECT_DOUBLE_EQ(3.0, tables.update(22, core::Gender::male, 9999, 3.0, rng));
    EXPECT_DOUBLE_EQ(3.0, tables.update(25, core::Gender::male, 2026, 3.0, rng));
    EXPECT_DOUBLE_EQ(4.0, tables.update(40, core::Gender::male, 2026, 4.0, rng));
}

TEST(JacardiEducation, UpgradeWithMissingCurrentThrows) {
    EducationLifecycleTables tables;
    tables.set_draw_at_22(
        {{EducationLifecycleTables::key_year_gender(2026, 0), make_stratum({3}, {1.0})}});
    tables.set_upgrades(
        {{EducationLifecycleTables::key_upgrade(2026, 25, 0, 3), make_stratum({4}, {1.0})}});
    Random rng;
    EXPECT_THROW(
        tables.update(25, core::Gender::female, 2026, EducationLifecycleTables::missing, rng),
        core::HgpsException);
}

TEST(JacardiEducation, PersonEducationHelpers) {
    Person person{core::Gender::female};
    EXPECT_FALSE(person_has_education(person));
    EXPECT_TRUE(std::isnan(get_person_education(person)));
    set_person_education(person, 5.0);
    EXPECT_TRUE(person_has_education(person));
    EXPECT_DOUBLE_EQ(5.0, get_person_education(person));

    // MAHIMA: NaN is stored but must count as missing (ages 0–5 / output skip).
    set_person_education(person, EducationLifecycleTables::missing);
    EXPECT_FALSE(person_has_education(person));
    EXPECT_TRUE(std::isnan(get_person_education(person)));
}
