/**
 * Expanded configuration and schema tests: old vs new config, version checks,
 * and POCO/JSON edge cases to improve coverage toward ~700 tests.
 */
#include "pch.h"

#include "HealthGPS.Input/configuration_parsing.h"
#include "HealthGPS.Input/configuration_parsing_helpers.h"
#include "HealthGPS.Input/jsonparser.h"
#include "HealthGPS.Input/poco.h"

#include "HealthGPS.Core/interval.h"

using json = nlohmann::json;
using namespace hgps::input;
using namespace hgps::core;

// --- Version: old config (v1) rejected, new (v2) only accepted ---
TEST(ConfigSchemaExpanded, Version2Accepted) {
    json j;
    j["version"] = 2;
    EXPECT_NO_THROW(check_version(j));
}

TEST(ConfigSchemaExpanded, Version1RejectedAsOldConfig) {
    json j;
    j["version"] = 1;
    EXPECT_THROW(check_version(j), ConfigurationError);
}

TEST(ConfigSchemaExpanded, Version0Rejected) {
    json j;
    j["version"] = 0;
    EXPECT_THROW(check_version(j), ConfigurationError);
}

TEST(ConfigSchemaExpanded, Version3Rejected) {
    json j;
    j["version"] = 3;
    EXPECT_THROW(check_version(j), ConfigurationError);
}

TEST(ConfigSchemaExpanded, VersionNegativeRejected) {
    json j;
    j["version"] = -1;
    EXPECT_THROW(check_version(j), ConfigurationError);
}

TEST(ConfigSchemaExpanded, VersionFloatCoercedToInt) {
    // get_to(j, "version", int) accepts 2.0 and coerces to 2, so version is accepted
    json j;
    j["version"] = 2.0;
    EXPECT_NO_THROW(check_version(j));
}

TEST(ConfigSchemaExpanded, VersionBoolRejected) {
    json j;
    j["version"] = true;
    EXPECT_THROW(check_version(j), ConfigurationError);
}

TEST(ConfigSchemaExpanded, VersionArrayRejected) {
    json j;
    j["version"] = json::array({2});
    EXPECT_THROW(check_version(j), ConfigurationError);
}

TEST(ConfigSchemaExpanded, VersionObjectRejected) {
    json j;
    j["version"] = json::object({{"major", 2}});
    EXPECT_THROW(check_version(j), ConfigurationError);
}

// --- get_to and get helpers ---
TEST(ConfigSchemaExpanded, GetToInt) {
    json j = {{"k", 42}};
    int v = 0;
    EXPECT_TRUE(get_to(j, "k", v));
    EXPECT_EQ(42, v);
}

TEST(ConfigSchemaExpanded, GetToUnsigned) {
    json j = {{"k", 100u}};
    unsigned int v = 0;
    EXPECT_TRUE(get_to(j, "k", v));
    EXPECT_EQ(100u, v);
}

TEST(ConfigSchemaExpanded, GetToString) {
    json j = {{"k", "hello"}};
    std::string v;
    EXPECT_TRUE(get_to(j, "k", v));
    EXPECT_EQ("hello", v);
}

TEST(ConfigSchemaExpanded, GetToBool) {
    json j = {{"k", true}};
    bool v = false;
    EXPECT_TRUE(get_to(j, "k", v));
    EXPECT_TRUE(v);
}

TEST(ConfigSchemaExpanded, GetToFloat) {
    json j = {{"k", 3.14f}};
    float v = 0;
    EXPECT_TRUE(get_to(j, "k", v));
    EXPECT_FLOAT_EQ(3.14f, v);
}

TEST(ConfigSchemaExpanded, GetToDouble) {
    json j = {{"k", 2.718}};
    double v = 0;
    EXPECT_TRUE(get_to(j, "k", v));
    EXPECT_DOUBLE_EQ(2.718, v);
}

TEST(ConfigSchemaExpanded, GetToMissingKeyReturnsFalse) {
    json j = {{"other", 1}};
    int v = 0;
    EXPECT_FALSE(get_to(j, "missing", v));
}

TEST(ConfigSchemaExpanded, GetToWrongTypeReturnsFalse) {
    json j = {{"k", "not_an_int"}};
    int v = 0;
    EXPECT_FALSE(get_to(j, "k", v));
}

TEST(ConfigSchemaExpanded, GetToSuccessFlagUnchangedOnSuccess) {
    json j = {{"k", 1}};
    int v = 0;
    bool flag = true;
    get_to(j, "k", v, flag);
    EXPECT_TRUE(flag);
}

TEST(ConfigSchemaExpanded, GetToSuccessFlagSetFalseOnFailure) {
    json j = {{"k", 1}};
    int v = 0;
    bool flag = true;
    get_to(j, "wrong_key", v, flag);
    EXPECT_FALSE(flag);
}

// --- IntegerInterval (config uses it for settings) ---
TEST(ConfigSchemaExpanded, SettingsInfoAgeRangeFromJson) {
    json j = {
        {"country_code", "UK"}, {"size_fraction", 0.01f}, {"age_range", json::array({18, 65})}};
    SettingsInfo s = j.get<SettingsInfo>();
    EXPECT_EQ(18, s.age_range.lower());
    EXPECT_EQ(65, s.age_range.upper());
}

TEST(ConfigSchemaExpanded, RiskFactorInfoOptionalRangeNull) {
    json j = {{"name", "BMI"}, {"level", 1}, {"range", nullptr}};
    RiskFactorInfo r = j.get<RiskFactorInfo>();
    EXPECT_EQ("BMI", r.name);
    EXPECT_EQ(1, r.level);
    EXPECT_FALSE(r.range.has_value());
}

TEST(ConfigSchemaExpanded, RiskFactorInfoWithRange) {
    json j = {{"name", "X"}, {"level", 0}, {"range", json::array({0.0, 1.0})}};
    RiskFactorInfo r = j.get<RiskFactorInfo>();
    ASSERT_TRUE(r.range.has_value());
    EXPECT_DOUBLE_EQ(0.0, r.range->lower());
    EXPECT_DOUBLE_EQ(1.0, r.range->upper());
}

TEST(ConfigSchemaExpanded, PolicyImpactInfoAllFields) {
    json j = {{"risk_factor", "BMI"}, {"impact_value", -0.5}, {"from_age", 10u}, {"to_age", 18u}};
    PolicyImpactInfo p = j.get<PolicyImpactInfo>();
    EXPECT_EQ("BMI", p.risk_factor);
    EXPECT_DOUBLE_EQ(-0.5, p.impact_value);
    EXPECT_EQ(10u, p.from_age);
    ASSERT_TRUE(p.to_age.has_value());
    EXPECT_EQ(18u, *p.to_age);
}

TEST(ConfigSchemaExpanded, PolicyImpactInfoToAgeNull) {
    json j = {{"risk_factor", "X"}, {"impact_value", 0.1}, {"from_age", 19u}, {"to_age", nullptr}};
    PolicyImpactInfo p = j.get<PolicyImpactInfo>();
    EXPECT_FALSE(p.to_age.has_value());
}

TEST(ConfigSchemaExpanded, IndividualIdTrackingConfigDefaults) {
    json j = json::object();
    IndividualIdTrackingConfig c = j.get<IndividualIdTrackingConfig>();
    EXPECT_FALSE(c.enabled);
    EXPECT_EQ("all", c.gender);
    EXPECT_EQ("both", c.scenarios);
}

TEST(ConfigSchemaExpanded, IndividualIdTrackingConfigGenderMale) {
    json j = {{"gender", "male"}};
    IndividualIdTrackingConfig c = j.get<IndividualIdTrackingConfig>();
    EXPECT_EQ("male", c.gender);
}

TEST(ConfigSchemaExpanded, IndividualIdTrackingConfigScenariosBaseline) {
    json j = {{"scenarios", "baseline"}};
    IndividualIdTrackingConfig c = j.get<IndividualIdTrackingConfig>();
    EXPECT_EQ("baseline", c.scenarios);
}

TEST(ConfigSchemaExpanded, IndividualIdTrackingConfigYearsEmpty) {
    json j = {{"years", json::array()}};
    IndividualIdTrackingConfig c = j.get<IndividualIdTrackingConfig>();
    EXPECT_TRUE(c.years.empty());
}

TEST(ConfigSchemaExpanded, IndividualIdTrackingConfigRegionsNonEmpty) {
    json j = {{"regions", json::array({"R1", "R2"})}};
    IndividualIdTrackingConfig c = j.get<IndividualIdTrackingConfig>();
    EXPECT_EQ(2u, c.regions.size());
    EXPECT_EQ("R1", c.regions[0]);
}

TEST(ConfigSchemaExpanded, OutputInfoComorbiditiesAndFolder) {
    json j = {{"folder", "/out"}, {"file_name", "f.txt"}, {"comorbidities", 5}};
    OutputInfo o = j.get<OutputInfo>();
    EXPECT_EQ(5u, o.comorbidities);
    EXPECT_EQ("/out", o.folder);
    EXPECT_EQ("f.txt", o.file_name);
}

TEST(ConfigSchemaExpanded, OutputInfoNoIndividualTracking) {
    json j = {{"folder", "/x"}, {"file_name", "x.txt"}, {"comorbidities", 1}};
    OutputInfo o = j.get<OutputInfo>();
    EXPECT_FALSE(o.individual_id_tracking.has_value());
}

TEST(ConfigSchemaExpanded, PolicyPeriodInfoStartOnly) {
    json j = {{"start_time", 2025}, {"finish_time", nullptr}};
    PolicyPeriodInfo p = j.get<PolicyPeriodInfo>();
    EXPECT_EQ(2025, p.start_time);
    EXPECT_FALSE(p.finish_time.has_value());
}

TEST(ConfigSchemaExpanded, PolicyPeriodInfoWithFinish) {
    json j = {{"start_time", 2020}, {"finish_time", 2050}};
    PolicyPeriodInfo p = j.get<PolicyPeriodInfo>();
    EXPECT_EQ(2050, *p.finish_time);
}

TEST(ConfigSchemaExpanded, PolicyAdjustmentInfoFromJson) {
    json j = {{"risk_factor", "sugar"}, {"value", 0.5}};
    PolicyAdjustmentInfo p = j.get<PolicyAdjustmentInfo>();
    EXPECT_EQ("sugar", p.risk_factor);
    EXPECT_DOUBLE_EQ(0.5, p.value);
}

TEST(ConfigSchemaExpanded, SESInfoParametersEmpty) {
    json j = {{"function_name", "normal"}, {"function_parameters", json::array()}};
    SESInfo s = j.get<SESInfo>();
    EXPECT_TRUE(s.parameters.empty());
}

TEST(ConfigSchemaExpanded, SESInfoParametersSingle) {
    json j = {{"function_name", "fixed"}, {"function_parameters", json::array({1.0})}};
    SESInfo s = j.get<SESInfo>();
    EXPECT_EQ(1u, s.parameters.size());
    EXPECT_DOUBLE_EQ(1.0, s.parameters[0]);
}

TEST(ConfigSchemaExpanded, CoefficientInfoAllFields) {
    json j = {{"value", 1.0}, {"stdError", 0.1}, {"tValue", 10.0}, {"pValue", 0.001}};
    CoefficientInfo c = j.get<CoefficientInfo>();
    EXPECT_DOUBLE_EQ(1.0, c.value);
    EXPECT_DOUBLE_EQ(0.1, c.std_error);
    EXPECT_DOUBLE_EQ(10.0, c.tvalue);
    EXPECT_DOUBLE_EQ(0.001, c.pvalue);
}

TEST(ConfigSchemaExpanded, VariableInfoFactorString) {
    json j = {{"Name", "Age"}, {"Factor", "1.0"}};
    VariableInfo v = j.get<VariableInfo>();
    EXPECT_EQ("1.0", v.factor);
}

TEST(ConfigSchemaExpanded, IntegerIntervalArrayTwoElements) {
    json j = json::array({5, 15});
    IntegerInterval i = j.get<IntegerInterval>();
    EXPECT_EQ(5, i.lower());
    EXPECT_EQ(15, i.upper());
}

TEST(ConfigSchemaExpanded, IntegerIntervalArrayOneElementThrows) {
    json j = json::array({5});
    EXPECT_THROW(j.get<IntegerInterval>(), json::type_error);
}

TEST(ConfigSchemaExpanded, IntegerIntervalArrayThreeElementsThrows) {
    json j = json::array({5, 10, 15});
    EXPECT_THROW(j.get<IntegerInterval>(), json::type_error);
}

TEST(ConfigSchemaExpanded, IntegerIntervalRoundTrip) {
    IntegerInterval a(10, 20);
    json j = a;
    IntegerInterval b = j.get<IntegerInterval>();
    EXPECT_EQ(a.lower(), b.lower());
    EXPECT_EQ(a.upper(), b.upper());
}

// --- ProjectRequirements default values (single-stage vs two-stage, demographics) ---
TEST(ConfigSchemaExpanded, ProjectRequirementsDefaultDemographics) {
    ProjectRequirements req{};
    EXPECT_TRUE(req.demographics.age);
    EXPECT_TRUE(req.demographics.gender);
    EXPECT_FALSE(req.demographics.region);
    EXPECT_FALSE(req.demographics.ethnicity);
    EXPECT_FALSE(req.demographics.max_age_for_linear_models.has_value());
}

TEST(ConfigSchemaExpanded, ProjectRequirementsDefaultTwoStage) {
    ProjectRequirements req{};
    EXPECT_FALSE(req.two_stage.use_logistic);
    EXPECT_TRUE(req.two_stage.logistic_file.empty());
}

TEST(ConfigSchemaExpanded, ProjectRequirementsDefaultIncome) {
    ProjectRequirements req{};
    EXPECT_TRUE(req.income.enabled);
    EXPECT_EQ("categorical", req.income.type);
    EXPECT_EQ("3", req.income.categories);
    EXPECT_TRUE(req.income.income_based_csv_output);
}

TEST(ConfigSchemaExpanded, ProjectRequirementsDefaultRiskFactors) {
    ProjectRequirements req{};
    EXPECT_TRUE(req.risk_factors.adjust_to_factors_mean);
    EXPECT_TRUE(req.risk_factors.trended);
}

TEST(ConfigSchemaExpanded, ProjectRequirementsDefaultTrend) {
    ProjectRequirements req{};
    EXPECT_FALSE(req.trend.enabled);
    EXPECT_EQ("null", req.trend.type);
}
