#include "HealthGPS.Input/jsonparser.h"
#include "HealthGPS.Input/poco.h"

#include "pch.h"

using namespace hgps::core;
using namespace hgps::input;
using json = nlohmann::json;

TEST(JsonParser, IntervalFromJson) {
    {
        // Successful parsing of regular interval
        IntegerInterval interval;
        const auto j = json::array({0, 1});
        EXPECT_NO_THROW(j.get_to(interval));
        EXPECT_EQ(interval, IntegerInterval(0, 1));
    }

    {
        // Fail to parse empty array
        IntegerInterval interval;
        const auto j = json::array();
        EXPECT_THROW(j.get_to(interval), json::type_error);
    }

    {
        // Fail to parse array with > 2 elements
        IntegerInterval interval;
        const auto j = json::array({0, 1, 2});
        EXPECT_THROW(j.get_to(interval), json::type_error);
    }
}

TEST(JsonParser, IntervalToJson) {
    // Convert to JSON and back again
    const IntegerInterval interval{0, 1};
    const json j = interval;
    EXPECT_EQ(j.get<IntegerInterval>(), interval);
}

// --- SettingsInfo ---
TEST(JsonParser, SettingsInfoRoundTrip) {
    SettingsInfo p{"UK", IntegerInterval(18, 65), 0.001f};
    json j = p;
    SettingsInfo q = j.get<SettingsInfo>();
    EXPECT_EQ(p.country, q.country);
    EXPECT_EQ(p.age_range, q.age_range);
    EXPECT_FLOAT_EQ(p.size_fraction, q.size_fraction);
}

TEST(JsonParser, SettingsInfoFromJsonMissingKeyThrows) {
    json j = {{"country_code", "X"}, {"size_fraction", 0.5}};  // missing age_range
    EXPECT_THROW(j.get<SettingsInfo>(), json::out_of_range);
}

// --- RiskFactorInfo ---
TEST(JsonParser, RiskFactorInfoRoundTrip) {
    RiskFactorInfo p{"BMI", 1, hgps::core::DoubleInterval(18.0, 30.0)};
    json j = p;
    RiskFactorInfo q = j.get<RiskFactorInfo>();
    EXPECT_EQ(p.name, q.name);
    EXPECT_EQ(p.level, q.level);
    EXPECT_TRUE(p.range.has_value() && q.range.has_value());
}

TEST(JsonParser, RiskFactorInfoRoundTripNoRange) {
    RiskFactorInfo p{"Gender", 0, std::nullopt};
    json j = p;
    RiskFactorInfo q = j.get<RiskFactorInfo>();
    EXPECT_EQ(p.name, q.name);
    EXPECT_FALSE(q.range.has_value());
}

// --- SESInfo ---
TEST(JsonParser, SESInfoRoundTrip) {
    SESInfo p{"normal", {0.0, 1.0}};
    json j = p;
    SESInfo q = j.get<SESInfo>();
    EXPECT_EQ(p.function, q.function);
    EXPECT_EQ(p.parameters, q.parameters);
}

TEST(JsonParser, SESInfoFromJsonMissingKeyThrows) {
    json j = {{"function_name", "normal"}};  // missing function_parameters
    EXPECT_THROW(j.get<SESInfo>(), json::out_of_range);
}

// --- PolicyPeriodInfo ---
TEST(JsonParser, PolicyPeriodInfoFromJsonWithNumericFinishTime) {
    // to_json writes finish_time as string; test from_json with numeric value
    json j = {{"start_time", 2020}, {"finish_time", 2050}};
    PolicyPeriodInfo q = j.get<PolicyPeriodInfo>();
    EXPECT_EQ(2020, q.start_time);
    ASSERT_TRUE(q.finish_time.has_value());
    EXPECT_EQ(2050, *q.finish_time);
}

TEST(JsonParser, PolicyPeriodInfoFromJsonNullFinishTime) {
    json j = {{"start_time", 2020}, {"finish_time", nullptr}};
    PolicyPeriodInfo q = j.get<PolicyPeriodInfo>();
    EXPECT_EQ(2020, q.start_time);
    EXPECT_FALSE(q.finish_time.has_value());
}

// --- PolicyImpactInfo ---
TEST(JsonParser, PolicyImpactInfoFromJsonWithToAge) {
    json j = {{"risk_factor", "BMI"}, {"impact_value", -0.5}, {"from_age", 10}, {"to_age", 18}};
    PolicyImpactInfo q = j.get<PolicyImpactInfo>();
    EXPECT_EQ("BMI", q.risk_factor);
    EXPECT_EQ(10u, q.from_age);
    ASSERT_TRUE(q.to_age.has_value());
    EXPECT_EQ(18u, *q.to_age);
}

TEST(JsonParser, PolicyImpactInfoFromJsonNullToAge) {
    json j = {{"risk_factor", "BMI"}, {"impact_value", -0.1}, {"from_age", 19}, {"to_age", nullptr}};
    PolicyImpactInfo q = j.get<PolicyImpactInfo>();
    EXPECT_FALSE(q.to_age.has_value());
}

// --- PolicyAdjustmentInfo ---
TEST(JsonParser, PolicyAdjustmentInfoRoundTrip) {
    PolicyAdjustmentInfo p{"sugar", 0.5};
    json j = p;
    PolicyAdjustmentInfo q = j.get<PolicyAdjustmentInfo>();
    EXPECT_EQ(p.risk_factor, q.risk_factor);
    EXPECT_DOUBLE_EQ(p.value, q.value);
}

// --- PolicyScenarioInfo ---
TEST(JsonParser, PolicyScenarioInfoFromJsonMinimal) {
    // Build JSON with numeric finish_time and one impact (to_json uses string for optionals)
    json j = {{"active_period", json::object({{"start_time", 2025}, {"finish_time", nullptr}})},
              {"impacts", json::array({{{"risk_factor", "BMI"},
                                       {"impact_value", -0.1},
                                       {"from_age", 0},
                                       {"to_age", nullptr}}})}};
    PolicyScenarioInfo q = j.get<PolicyScenarioInfo>();
    EXPECT_EQ(2025, q.active_period.start_time);
    EXPECT_EQ(1u, q.impacts.size());
}

TEST(JsonParser, PolicyScenarioInfoFromJsonMissingImpactsThrows) {
    json j = {{"active_period", json::object({{"start_time", 2020}, {"finish_time", nullptr}})}};
    EXPECT_THROW(j.get<PolicyScenarioInfo>(), json::out_of_range);
}

// --- IndividualIdTrackingConfig ---
TEST(JsonParser, IndividualIdTrackingConfigRoundTrip) {
    IndividualIdTrackingConfig p{};
    p.enabled = true;
    p.age_min = 25;
    p.age_max = 60;
    p.gender = "female";
    p.risk_factors = {"bmi", "smoking"};
    p.years = {2030, 2040};
    p.scenarios = "both";
    json j = p;
    IndividualIdTrackingConfig q = j.get<IndividualIdTrackingConfig>();
    EXPECT_EQ(p.enabled, q.enabled);
    EXPECT_EQ(p.age_min, q.age_min);
    EXPECT_EQ(p.age_max, q.age_max);
    EXPECT_EQ(p.gender, q.gender);
    EXPECT_EQ(p.risk_factors, q.risk_factors);
    EXPECT_EQ(p.years, q.years);
    EXPECT_EQ(p.scenarios, q.scenarios);
}

TEST(JsonParser, IndividualIdTrackingConfigRoundTripOptionalAgesOmitted) {
    IndividualIdTrackingConfig p{};
    p.enabled = false;
    json j = p;
    IndividualIdTrackingConfig q = j.get<IndividualIdTrackingConfig>();
    EXPECT_FALSE(q.enabled);
    EXPECT_FALSE(q.age_min.has_value());
    EXPECT_FALSE(q.age_max.has_value());
}

// --- OutputInfo ---
TEST(JsonParser, OutputInfoRoundTripWithoutIndividualTracking) {
    OutputInfo p{3, "/out", "results.txt", std::nullopt};
    json j = p;
    OutputInfo q = j.get<OutputInfo>();
    EXPECT_EQ(p.comorbidities, q.comorbidities);
    EXPECT_EQ(p.folder, q.folder);
    EXPECT_EQ(p.file_name, q.file_name);
    EXPECT_FALSE(q.individual_id_tracking.has_value());
}

TEST(JsonParser, OutputInfoRoundTripWithIndividualTracking) {
    IndividualIdTrackingConfig track{};
    track.enabled = true;
    track.scenarios = "baseline";
    OutputInfo p{2, "/tmp", "out.csv", track};
    json j = p;
    OutputInfo q = j.get<OutputInfo>();
    ASSERT_TRUE(q.individual_id_tracking.has_value());
    EXPECT_TRUE(q.individual_id_tracking->enabled);
    EXPECT_EQ(q.individual_id_tracking->scenarios, "baseline");
}

TEST(JsonParser, OutputInfoFromJsonMissingFileNameThrows) {
    json j = {{"folder", "/x"}, {"comorbidities", 3}};
    EXPECT_THROW(j.get<OutputInfo>(), json::out_of_range);
}

// --- CoefficientInfo (risk model) ---
TEST(JsonParser, CoefficientInfoRoundTrip) {
    CoefficientInfo p{1.5, 0.1, 15.0, 0.001};
    json j = p;
    CoefficientInfo q = j.get<CoefficientInfo>();
    EXPECT_DOUBLE_EQ(p.value, q.value);
    EXPECT_DOUBLE_EQ(p.std_error, q.std_error);
}

// --- LinearModelInfo ---
TEST(JsonParser, LinearModelInfoRoundTrip) {
    LinearModelInfo p;
    p.formula = "y ~ x";
    p.coefficients["c0"] = CoefficientInfo{1.0, 0.0, 0.0, 0.0};
    p.residuals_standard_deviation = 0.5;
    p.rsquared = 0.9;
    json j = p;
    LinearModelInfo q = j.get<LinearModelInfo>();
    EXPECT_EQ(p.formula, q.formula);
    EXPECT_EQ(p.coefficients.size(), q.coefficients.size());
}

// --- VariableInfo ---
TEST(JsonParser, VariableInfoRoundTrip) {
    VariableInfo p{"Age", "1.0"};
    json j = p;
    VariableInfo q = j.get<VariableInfo>();
    EXPECT_EQ(p.name, q.name);
    EXPECT_EQ(p.factor, q.factor);
}

// --- FactorDynamicEquationInfo ---
TEST(JsonParser, FactorDynamicEquationInfoRoundTrip) {
    FactorDynamicEquationInfo p;
    p.name = "BMI";
    p.coefficients["a"] = 0.1;
    p.coefficients["b"] = 0.2;
    p.residuals_standard_deviation = 0.5;
    json j = p;
    FactorDynamicEquationInfo q = j.get<FactorDynamicEquationInfo>();
    EXPECT_EQ(p.name, q.name);
    EXPECT_EQ(p.coefficients.size(), q.coefficients.size());
    EXPECT_DOUBLE_EQ(p.residuals_standard_deviation, q.residuals_standard_deviation);
}

// --- DoubleInterval (core) ---
TEST(JsonParser, DoubleIntervalRoundTrip) {
    const hgps::core::DoubleInterval interval(18.0, 65.0);
    json j = interval;
    EXPECT_EQ(j.get<hgps::core::DoubleInterval>().lower(), 18.0);
    EXPECT_EQ(j.get<hgps::core::DoubleInterval>().upper(), 65.0);
}

TEST(JsonParser, DoubleIntervalFromJsonWrongSizeThrows) {
    json j = json::array({1.0});
    EXPECT_THROW(j.get<hgps::core::DoubleInterval>(), json::type_error);
}

// --- Array2Info ---
TEST(JsonParser, Array2InfoRoundTrip) {
    Array2Info p;
    p.rows = 2;
    p.cols = 3;
    p.data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    json j = p;
    Array2Info q = j.get<Array2Info>();
    EXPECT_EQ(p.rows, q.rows);
    EXPECT_EQ(p.cols, q.cols);
    EXPECT_EQ(p.data, q.data);
}

TEST(JsonParser, Array2InfoFromJsonMissingRowsThrows) {
    json j = {{"cols", 2}, {"data", json::array({1.0, 2.0})}};
    EXPECT_THROW(j.get<Array2Info>(), json::out_of_range);
}

// --- FileInfo to_json (no from_json in parser; used by get_file_info) ---
TEST(JsonParser, FileInfoToJson) {
    FileInfo p;
    p.name = "/tmp/file.csv";
    p.format = "csv";
    p.delimiter = ",";
    p.columns = {{"a", "int"}, {"b", "string"}};
    json j = p;
    EXPECT_EQ(p.name.string(), j["name"].get<std::string>());
    EXPECT_EQ(p.format, j["format"].get<std::string>());
    EXPECT_EQ(p.delimiter, j["delimiter"].get<std::string>());
    EXPECT_EQ(2u, j["columns"].size());
}
