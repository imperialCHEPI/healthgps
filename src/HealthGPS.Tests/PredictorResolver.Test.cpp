#include "pch.h"

#include "HealthGPS/linear_model_evaluator.h"
#include "HealthGPS/person.h"
#include "HealthGPS/predictor_resolver.h"
#include <cmath>
#include <gtest/gtest.h>

namespace {
double expect_resolved(const hgps::Person &person, const std::string &key) {
    const auto value = hgps::resolve_derived_predictor(person, key);
    EXPECT_TRUE(value.has_value()) << key;
    return value.value_or(0.0);
}
} // namespace

TEST(TestHealthGPS_PredictorResolver, IncomePolynomialAndLog) {
    using namespace hgps;

    Person person;
    person.risk_factors["income"_id] = 100.0;

    EXPECT_DOUBLE_EQ(100.0, expect_resolved(person, "income"));
    EXPECT_DOUBLE_EQ(10000.0, expect_resolved(person, "income2"));
    EXPECT_NEAR(std::log(100.0), expect_resolved(person, "log_income"), 1e-9);
    const double log_income = std::log(100.0);
    EXPECT_NEAR(log_income * log_income, expect_resolved(person, "log_income2"), 1e-9);
}

TEST(TestHealthGPS_PredictorResolver, RegionDummy) {
    using namespace hgps;

    Person person;
    person.region = "region2";

    EXPECT_DOUBLE_EQ(1.0, expect_resolved(person, "region2"));
    EXPECT_DOUBLE_EQ(0.0, expect_resolved(person, "region3"));
}

TEST(TestHealthGPS_PredictorResolver, GetRiskFactorValueUsesResolver) {
    using namespace hgps;

    Person person;
    person.risk_factors["income"_id] = 10.0;

    EXPECT_DOUBLE_EQ(100.0, person.get_risk_factor_value("income2"_id));
}

TEST(TestHealthGPS_PredictorResolver, UnknownPredictorThrows) {
    using namespace hgps;

    Person person;
    EXPECT_THROW(person.get_risk_factor_value("not_a_real_predictor"_id), std::out_of_range);
}

TEST(TestHealthGPS_LinearModelEvaluator, SkipsMetadataRows) {
    using namespace hgps;

    Person person;
    person.age = 20;
    person.gender = core::Gender::male;
    person.region = "region1";
    person.ethnicity = "ethnicity1";

    LinearModelParams model;
    model.intercept = 5.0;
    model.coefficients["age1"_id] = 2.0;
    model.coefficients["stddev"_id] = 99.0;
    model.coefficients["min"_id] = 1.0;

    const double linear = evaluate_linear_model(person, model);
    EXPECT_DOUBLE_EQ(5.0 + (2.0 * 20.0), linear);
}

TEST(TestHealthGPS_LinearModelEvaluator, CappedAgeOption) {
    using namespace hgps;

    Person person;
    person.age = 100;

    LinearModelParams model;
    model.intercept = 0.0;
    model.coefficients["age2"_id] = 1.0;

    LinearModelEvalOptions options;
    options.capped_age = 50.0;

    EXPECT_DOUBLE_EQ(2500.0, evaluate_linear_model(person, model, options));
}

TEST(TestHealthGPS_LinearModelEvaluator, CappedAgePredictorDirect) {
    using namespace hgps;

    Person person;
    person.age = 100;

    LinearModelEvalOptions options;
    options.capped_age = 40.0;

    EXPECT_DOUBLE_EQ(40.0, get_linear_predictor_value(person, "age"_id, options));
    EXPECT_DOUBLE_EQ(1600.0, get_linear_predictor_value(person, "age2"_id, options));
    EXPECT_DOUBLE_EQ(64000.0, get_linear_predictor_value(person, "age3"_id, options));
}

TEST(TestHealthGPS_LinearModelEvaluator, LogCoefficientsAndFallback) {
    using namespace hgps;

    Person person;
    person.risk_factors["foodcarbohydrate"_id] = 4.0;

    LinearModelParams model;
    model.intercept = 1.0;
    model.log_coefficients["foodcarbohydrate"_id] = 2.0;
    model.coefficients["missing_factor"_id] = 3.0;

    LinearModelEvalOptions options;
    options.missing_predictor_fallback = [](const core::Identifier &name) -> std::optional<double> {
        if (name == "missing_factor"_id) {
            return 5.0;
        }
        return std::nullopt;
    };

    const double expected = 1.0 + (2.0 * std::log(4.0)) + (3.0 * 5.0);
    EXPECT_NEAR(expected, evaluate_linear_model(person, model, options), 1e-9);
}

TEST(TestHealthGPS_LinearModelEvaluator, LogCoefficientUsesFloorForNonPositive) {
    using namespace hgps;

    Person person;
    person.risk_factors["foodcarbohydrate"_id] = 0.0;

    LinearModelParams model;
    model.log_coefficients["foodcarbohydrate"_id] = 1.0;

    const double linear = evaluate_linear_model(person, model);
    EXPECT_NEAR(std::log(1e-10), linear, 1e-12);
}

TEST(TestHealthGPS_PredictorResolver, AgeEthnicityGenderSector) {
    using namespace hgps;

    Person person;
    person.age = 5;
    person.gender = core::Gender::male;
    person.ethnicity = "ethnicity3";
    person.region = "region1";
    person.sector = core::Sector::urban;

    EXPECT_DOUBLE_EQ(5.0, expect_resolved(person, "age"));
    EXPECT_DOUBLE_EQ(25.0, expect_resolved(person, "age2"));
    EXPECT_DOUBLE_EQ(1.0, expect_resolved(person, "ethnicity3"));
    EXPECT_DOUBLE_EQ(0.0, expect_resolved(person, "ethnicity2"));
    EXPECT_DOUBLE_EQ(1.0, expect_resolved(person, "gender2"));
    EXPECT_DOUBLE_EQ(person.gender_to_value(), expect_resolved(person, "gender"));
    EXPECT_DOUBLE_EQ(person.sector_to_value(), expect_resolved(person, "sector"));
    EXPECT_DOUBLE_EQ(std::pow(person.sector_to_value(), 2), expect_resolved(person, "sector2"));
}

TEST(TestHealthGPS_PredictorResolver, IncomeFromContinuousWhenRiskFactorMissing) {
    using namespace hgps;

    Person person;
    person.income_continuous = 42.0;

    EXPECT_DOUBLE_EQ(42.0, expect_resolved(person, "income"));
    EXPECT_DOUBLE_EQ(42.0 * 42.0, expect_resolved(person, "income2"));
}

TEST(TestHealthGPS_PredictorResolver, MetadataPredictorReturnsNullopt) {
    using namespace hgps;

    Person person;
    EXPECT_FALSE(resolve_derived_predictor(person, "stddev").has_value());
    EXPECT_FALSE(resolve_derived_predictor(person, "Intercept").has_value());
    EXPECT_TRUE(is_metadata_predictor(std::string("min")));
    EXPECT_TRUE(is_metadata_predictor(core::Identifier("lambda")));
}
