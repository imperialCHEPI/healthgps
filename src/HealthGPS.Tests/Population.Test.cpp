#include "pch.h"

#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/api.h"
#include "HealthGPS/person.h"
#include "HealthGPS/static_linear_model.h"
#include "HealthGPS/two_step_value.h"
#include <gtest/gtest.h>

TEST(TestHealthGPS_Population, CreateDefaultPerson) {
    using namespace hgps;

    auto p = Person{};
    ASSERT_GT(p.id(), 0);
    ASSERT_EQ(0u, p.age);
    ASSERT_EQ(core::Gender::unknown, p.gender);
    ASSERT_TRUE(p.is_alive());
    ASSERT_FALSE(p.has_emigrated());
    ASSERT_TRUE(p.is_active());
    ASSERT_EQ(0u, p.time_of_death());
    ASSERT_EQ(0u, p.time_of_migration());
    ASSERT_EQ(0, p.ses);
    ASSERT_EQ(0, p.risk_factors.size());
    ASSERT_EQ(0, p.diseases.size());
}

TEST(TestHealthGPS_Population, CreateUniquePerson) {
    using namespace hgps;

    auto p1 = Person{};
    auto p2 = Person{};
    const auto &p3 = p1;

    ASSERT_GT(p1.id(), 0);
    ASSERT_GT(p2.id(), p1.id());
    ASSERT_EQ(p1.id(), p3.id());
}

TEST(TestHealthGPS_Population, PersonStateIsActive) {
    using namespace hgps;

    auto p1 = Person{};
    auto p2 = Person{};
    auto p3 = Person{};
    auto p4 = p2;

    p2.die(2022);
    p3.emigrate(2022);

    ASSERT_TRUE(p1.is_active());
    ASSERT_FALSE(p2.is_active());
    ASSERT_FALSE(p3.is_active());
    ASSERT_TRUE(p4.is_active());
}

TEST(TestHealthGPS_Population, PersonStateDeath) {
    using namespace hgps;

    auto p = Person{};
    ASSERT_TRUE(p.is_alive());
    ASSERT_TRUE(p.is_active());
    ASSERT_FALSE(p.has_emigrated());
    ASSERT_EQ(0u, p.time_of_death());
    ASSERT_EQ(0u, p.time_of_migration());

    auto time_now = 2022u;
    p.die(time_now);

    ASSERT_FALSE(p.is_alive());
    ASSERT_FALSE(p.is_active());
    ASSERT_FALSE(p.has_emigrated());
    ASSERT_EQ(time_now, p.time_of_death());
    ASSERT_EQ(0u, p.time_of_migration());
    ASSERT_THROW(p.die(time_now), std::logic_error);
    ASSERT_THROW(p.emigrate(time_now), std::logic_error);
}

TEST(TestHealthGPS_Population, PersonStateEmigrated) {
    using namespace hgps;

    auto p = Person{};
    ASSERT_TRUE(p.is_alive());
    ASSERT_TRUE(p.is_active());
    ASSERT_FALSE(p.has_emigrated());
    ASSERT_EQ(0u, p.time_of_death());
    ASSERT_EQ(0u, p.time_of_migration());

    auto time_now = 2022u;
    p.emigrate(time_now);

    ASSERT_TRUE(p.is_alive());
    ASSERT_FALSE(p.is_active());
    ASSERT_TRUE(p.has_emigrated());
    ASSERT_EQ(0u, p.time_of_death());
    ASSERT_EQ(time_now, p.time_of_migration());
    ASSERT_THROW(p.die(time_now), std::logic_error);
    ASSERT_THROW(p.emigrate(time_now), std::logic_error);
}

TEST(TestHealthGPS_Population, CreateDefaultTwoStepValue) {
    using namespace hgps;

    auto v = TwoStepValue<int>{};
    ASSERT_EQ(0, v());
    ASSERT_EQ(0, v.value());
    ASSERT_EQ(0, v.old_value());
}

TEST(TestHealthGPS_Population, CreateCustomTwoStepValue) {
    using namespace hgps;

    auto v = TwoStepValue<int>{5};
    ASSERT_EQ(5, v());
    ASSERT_EQ(5, v.value());
    ASSERT_EQ(0, v.old_value());
}

TEST(TestHealthGPS_Population, AssignToTwoStepValue) {
    using namespace hgps;

    auto v = TwoStepValue<int>{5};
    v = 10;
    v.set_value(v.value() + 3);

    ASSERT_EQ(13, v());
    ASSERT_EQ(13, v.value());
    ASSERT_EQ(10, v.old_value());
}

TEST(TestHealthGPS_Population, SetBothTwoStepValues) {
    using namespace hgps;

    auto v = TwoStepValue<int>{};
    v.set_both_values(15);

    ASSERT_EQ(15, v());
    ASSERT_EQ(15, v.value());
    ASSERT_EQ(15, v.old_value());
}

TEST(TestHealthGPS_Population, CloneTwoStepValues) {
    using namespace hgps;

    auto source = TwoStepValue<int>{5};
    source = 10;

    auto clone = source.clone();

    ASSERT_EQ(source(), clone());
    ASSERT_EQ(source.value(), clone.value());
    ASSERT_EQ(source.old_value(), clone.old_value());
    ASSERT_NE(std::addressof(source), std::addressof(clone));
}

TEST(TestHealthGPS_Population, CreateDefaultDisease) {
    using namespace hgps;

    auto v = Disease{};

    ASSERT_EQ(DiseaseStatus::free, v.status);
    ASSERT_EQ(0, v.start_time);
}

TEST(TestHealthGPS_Population, CloneDiseaseType) {
    using namespace hgps;

    auto source = Disease{.status = DiseaseStatus::active, .start_time = 2021};
    auto clone = source.clone();

    ASSERT_EQ(source.status, clone.status);
    ASSERT_EQ(source.start_time, clone.start_time);
    ASSERT_NE(std::addressof(source), std::addressof(clone));
}

TEST(TestHealthGPS_Population, AddSingleNewEntity) {
    using namespace hgps;

    constexpr auto init_size = 10;

    auto p = Population{init_size};
    ASSERT_EQ(p.initial_size(), p.current_active_size());

    auto time_now = 2022;
    auto start_size = p.size();
    p.add(Person{core::Gender::male}, time_now);
    ASSERT_GT(p.size(), start_size);

    p[start_size].die(time_now);
    ASSERT_FALSE(p[start_size].is_active());

    time_now++;
    auto current_size = p.size();
    p.add(Person{core::Gender::female}, time_now);
    ASSERT_EQ(p.size(), current_size);
    ASSERT_TRUE(p[start_size].is_active());
}

TEST(TestHealthGPS_Population, AddMultipleNewEntities) {
    using namespace hgps;

    constexpr auto init_size = 10;
    constexpr auto alocate_size = 3;
    constexpr auto replace_size = 2;
    constexpr auto expected_size = init_size + alocate_size;

    static_assert(replace_size < alocate_size);

    auto p = Population{init_size};
    ASSERT_EQ(p.initial_size(), p.current_active_size());

    auto time_now = 2022;
    auto start_size = p.size();
    auto midpoint = start_size / 2;
    p.add_newborn_babies(alocate_size, core::Gender::male, time_now);
    ASSERT_GT(p.size(), start_size);

    p[midpoint].emigrate(time_now);
    p[start_size].die(time_now);
    ASSERT_FALSE(p[midpoint].is_active());
    ASSERT_FALSE(p[start_size].is_active());

    time_now++;
    auto current_size = p.size();
    p.add_newborn_babies(replace_size, core::Gender::female, time_now);
    ASSERT_EQ(current_size, p.size());
    ASSERT_EQ(expected_size, p.size());
    ASSERT_TRUE(p[midpoint].is_active());
    ASSERT_TRUE(p[start_size].is_active());
}

TEST(TestHealthGPS_Population, PersonIncomeValues) {
    using namespace hgps;

    auto p = Person{};
    p.income = core::Income::low;
    ASSERT_EQ(1.0f, p.income_to_value());

    p.income = core::Income::lowermiddle;
    ASSERT_EQ(2.0f, p.income_to_value());

    p.income = core::Income::uppermiddle;
    ASSERT_EQ(3.0f, p.income_to_value());

    p.income = core::Income::high;
    ASSERT_EQ(4.0f, p.income_to_value());

    p.income = core::Income::unknown;
    ASSERT_THROW(p.income_to_value(), core::HgpsException);
}

TEST(TestHealthGPS_Population, RegionModelOperations) {
    using namespace hgps;

    auto p = Person{};
    p.region = core::Region::England;
    ASSERT_EQ(1.0f, p.region_to_value());

    p.region = core::Region::Wales;
    ASSERT_EQ(2.0f, p.region_to_value());

    p.region = core::Region::Scotland;
    ASSERT_EQ(3.0f, p.region_to_value());

    p.region = core::Region::NorthernIreland;
    ASSERT_EQ(4.0f, p.region_to_value());

    p.region = core::Region::unknown;
    ASSERT_THROW(p.region_to_value(), core::HgpsException);
}

TEST(TestHealthGPS_Population, RegionModelParsing) {
    using namespace hgps;
    using namespace hgps::input;

    ASSERT_EQ(core::Region::England, parse_region("England"));
    ASSERT_EQ(core::Region::Wales, parse_region("Wales"));
    ASSERT_EQ(core::Region::Scotland, parse_region("Scotland"));
    ASSERT_EQ(core::Region::NorthernIreland, parse_region("NorthernIreland"));
    ASSERT_THROW(parse_region("Invalid"), core::HgpsException);
}

TEST(TestHealthGPS_Population, PersonRegionCloning) {
    using namespace hgps;

    auto source = Person{};
    source.region = core::Region::England;
    source.age = 25;
    source.gender = core::Gender::male;
    source.ses = 0.5;
    source.sector = core::Sector::urban;
    source.income = core::Income::high;

    auto clone = Person{};
    clone.region = source.region;
    clone.age = source.age;
    clone.gender = source.gender;
    clone.ses = source.ses;
    clone.sector = source.sector;
    clone.income = source.income;

    ASSERT_EQ(clone.region, source.region);
    ASSERT_EQ(clone.region_to_value(), source.region_to_value());
    ASSERT_EQ(clone.age, source.age);
    ASSERT_EQ(clone.gender, source.gender);
    ASSERT_EQ(clone.ses, source.ses);
    ASSERT_EQ(clone.sector, source.sector);
    ASSERT_EQ(clone.income, source.income);
}
