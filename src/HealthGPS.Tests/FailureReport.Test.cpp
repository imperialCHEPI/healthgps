#include "pch.h"

#include "HealthGPS/disease_table.h"
#include "HealthGPS/failure_report.h"
#include "HealthGPS/person.h"

#include <stdexcept>
#include <string>

namespace {

struct ClearPhaseOnExit {
    ~ClearPhaseOnExit() { hgps::clear_simulation_phase(); }
};

} // namespace

TEST(TestFailureReport, ZeroAndTwoThreadsAreAllowed) {
    EXPECT_NO_THROW(hgps::validate_requested_thread_count(0));
    EXPECT_NO_THROW(hgps::validate_requested_thread_count(hgps::minimum_explicit_thread_count));
    EXPECT_NO_THROW(hgps::validate_requested_thread_count(4));
}

TEST(TestFailureReport, OneThreadExplainsTheMinimum) {
    try {
        hgps::validate_requested_thread_count(1);
        FAIL() << "one thread must be rejected";
    } catch (const std::invalid_argument &ex) {
        const auto text = std::string{ex.what()};
        EXPECT_NE(text.find("-T 1"), std::string::npos);
        EXPECT_NE(text.find("minimum of 2"), std::string::npos);
    }
}

TEST(TestFailureReport, UnknownFailureNamesThePhase) {
    const auto message = hgps::format_unknown_failure("risk_factor");
    EXPECT_NE(message.find("risk_factor"), std::string::npos);
    EXPECT_NE(message.find("does not derive from"), std::string::npos);
}

TEST(TestFailureReport, FormatFailureIncludesPhaseAndReason) {
    const auto message =
        hgps::format_failure("disease > initialise_disease_status 'cervicalcancer'",
                             std::out_of_range{"Relative risk for factor 'BMI' has no row"});
    EXPECT_NE(message.find("cervicalcancer"), std::string::npos);
    EXPECT_NE(message.find("std::out_of_range"), std::string::npos);
    EXPECT_NE(message.find("BMI"), std::string::npos);
}

TEST(TestFailureReport, PhasePathSurvivesTheThrow) {
    const ClearPhaseOnExit clear;
    try {
        hgps::SimulationPhase outer{"initialise_population"};
        {
            hgps::SimulationPhase inner{"initialise_disease_status 'cervicalcancer'"};
            throw std::runtime_error{"missing BMI row"};
        }
    } catch (const std::runtime_error &) {
        const auto phase = hgps::current_simulation_phase();
        EXPECT_NE(phase.find("initialise_population"), std::string::npos);
        EXPECT_NE(phase.find("cervicalcancer"), std::string::npos);
    }
}

TEST(TestFailureReport, PhasePathClearsAfterNormalReturn) {
    const ClearPhaseOnExit clear;
    {
        hgps::SimulationPhase outer{"initialise_population"};
        {
            hgps::SimulationPhase inner{"demographic"};
            EXPECT_NE(hgps::current_simulation_phase().find("demographic"), std::string::npos);
        }
        EXPECT_EQ(hgps::current_simulation_phase(), "initialise_population");
    }
    EXPECT_TRUE(hgps::current_simulation_phase().empty());
}

TEST(TestFailureReport, MissingRiskFactorNamesThePerson) {
    hgps::Person person{hgps::core::Gender::male};
    person.age = 0;
    try {
        (void)person.get_risk_factor_value(hgps::core::Identifier{"bmi"});
        FAIL() << "bmi is not on the person";
    } catch (const std::out_of_range &ex) {
        const auto text = std::string{ex.what()};
        EXPECT_NE(text.find("bmi"), std::string::npos);
        EXPECT_NE(text.find("age 0"), std::string::npos);
        EXPECT_NE(text.find("male"), std::string::npos);
        EXPECT_NE(text.find("entirely null"), std::string::npos);
    }
}

TEST(TestFailureReport, DiseaseTableNamesMissingSexAndMeasure) {
    using namespace hgps;
    core::DiseaseInfo info;
    info.code = core::Identifier{"cervicalcancer"};
    info.name = "Cervical cancer";

    std::map<std::string, int> measures{{"prevalence", 0}};
    std::map<int, std::map<core::Gender, DiseaseMeasure>> rows;
    rows[40][core::Gender::female] = DiseaseMeasure{std::map<int, double>{{0, 0.01}}};
    DiseaseTable table{info, std::move(measures), std::move(rows)};

    try {
        (void)table(40, core::Gender::male);
        FAIL() << "male row is absent";
    } catch (const std::out_of_range &ex) {
        const auto text = std::string{ex.what()};
        EXPECT_NE(text.find("cervicalcancer"), std::string::npos);
        EXPECT_NE(text.find("male"), std::string::npos);
        EXPECT_NE(text.find("40"), std::string::npos);
    }

    try {
        (void)table.at("incidence");
        FAIL() << "incidence column is absent";
    } catch (const std::out_of_range &ex) {
        const auto text = std::string{ex.what()};
        EXPECT_NE(text.find("cervicalcancer"), std::string::npos);
        EXPECT_NE(text.find("incidence"), std::string::npos);
    }
}
