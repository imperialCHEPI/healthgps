#include "pch.h"

#include "HealthGPS/linear_model_evaluator.h"
#include "HealthGPS/person.h"
#include "HealthGPS/predictor_resolver.h"
#include <cmath>
#include <gtest/gtest.h>

TEST(TestHealthGPS_PredictorResolver, IncomePolynomialAndLog) {
    using namespace hgps;

    Person person;
    person.risk_factors["income"_id] = 100.0;

    EXPECT_DOUBLE_EQ(100.0, resolve_derived_predictor(person, "income").value());
    EXPECT_DOUBLE_EQ(10000.0, resolve_derived_predictor(person, "income2").value());
    EXPECT_NEAR(std::log(100.0), resolve_derived_predictor(person, "log_income").value(), 1e-9);
    const double log_income = std::log(100.0);
    EXPECT_NEAR(log_income * log_income, resolve_derived_predictor(person, "log_income2").value(),
                1e-9);
}

TEST(TestHealthGPS_PredictorResolver, RegionDummy) {
    using namespace hgps;

    Person person;
    person.region = "region2";

    EXPECT_DOUBLE_EQ(1.0, resolve_derived_predictor(person, "region2").value());
    EXPECT_DOUBLE_EQ(0.0, resolve_derived_predictor(person, "region3").value());
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
    EXPECT_DOUBLE_EQ(5.0 + 2.0 * 20.0, linear);
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
