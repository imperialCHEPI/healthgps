#include "pch.h"

#include "HealthGPS/static_linear_model.h"

auto expected = hgps::RiskFactorSexAgeTable{};

std::vector<double> factorsMale = {200.,40.,30.,2000.,1.,1000.,3.,50.};
std::vector<double> factorsFemale = {200.,40.,30.,2000.,1.,1000.,3.,50.};

std::unordered_map<hgps::core::Identifier, std::vector<double>> factorsMale = {
    {hgps::core::Identifier{"0"}, factorsMale},
    {hgps::core::Identifier{"1"}, factorsMale}
};

std::unordered_map<hgps::core::Identifier, std::vector<double>> factorsFemale = {
    {hgps::core::Identifier{"0"}, factorsFemale},
    {hgps::core::Identifier{"1"}, factorsFemale}
};

expected.emplace_row(hgps::core::Gender::male, factorsMale);
expected.emplace_row(hgps::core::Gender::female, factorsFemale);
std::vector<hgps::core::Identifier> names = {"FoodCarbohydrate", "FoodFat"};

// Define models
// We will define 2 RiskFactorModels: carb_model and fat_model 
std::vector<hgps::LinearModelParams> models;
hgps::LinearModelParams carb_model;
carb_model.intercept = -0.241505734056409;
std::unordered_map<hgps::core::Identifier, double> coefficients = {
    {"Gender", 0.00191412437431455},
    {"Age", -0.000545257990453302},
    {"Age2", 3.06434596341924e-6},
    {"Age3", 5.24468215546687e-9},
    {"Sector", 0.0967308121487847},
    {"Income", 0.0810023788842083}
};
carb_model.coefficients = coefficients;

hgps::LinearModelParams fat_model;
fat_model.intercept = -0.693521187677;
coefficients = {
    {"Gender", 0.00385126434589752},
    {"Age", -0.00433711587715954},
    {"Age2", 3.11799231385975e-5},
    {"Age3", 3.4291809335544e-8},
    {"Sector", -0.0629531974852436},
    {"Income", 0.373256996730791}
};
fat_model.coefficients = coefficients;

models.emplace_back(std::move(carb_model));
models.emplace_back(std::move(fat_model));

std::vector<double> lambda = {0.262626262626263, 0.141414141414141};
std::vector<double> stddev = {0.337810256621877, 0.436763527499362};

Eigen::MatrixXd correlation{2, 2};
hgps::core::DataTable correlation_table;
// I really don't know how to build this correlation table.
// The data I want to use looks like this in a csv file:
// "FoodCarbohydrate","FoodFat"
// 1.0,0.322065425
// 0.322065425,1.0

// Define policy models
std::vector<hgps::LinearModelParams> policy_models;
std::vector<hgps::core::DoubleInterval> policy_ranges;

// We need 2 policy models for the 2 risk factor models
hgps::LinearModelParams carb_policy_model;
carb_policy_model.intercept = -1.29385215316281;
coefficients = {
    {"Gender", 0.00845121355575458},
    {"Age", 0.000386003599279214},
    {"Sector", 0.0621763530502617},
    {"Income", 0.0560558815729017}
};
carb_policy_model.coefficients = coefficients;
std::unordered_map<hgps::core::Identifier, double> log_coefficients = {
    {"FoodCarbohydrate", 0.577638667323293},
    {"FoodFat", -0.0801300827856402},
    {"FoodProtein", -0.480946893299471},
    {"FoodSodium", -0.0396319735508116}
};
carb_policy_model.log_coefficients = log_coefficients;
policy_ranges.emplace_back(hgps::core::DoubleInterval{-2.31063, 0.37526});

hgps::LinearModelParams fat_policy_model;
fat_policy_model.intercept = -0.734121205356728;
coefficients = {

}

policy_models.emplace_back(std::move(policy_model));
// Check intervention policy covariance matrix column name matches risk factor name.
auto policy_column_name = policy_covariance_table.column(i).name();
if (!hgps::core::case_insensitive::equals(key, policy_column_name)) {
    throw hgps::core::HgpsException{
        fmt::format("Risk factor {} name ({}) does not match intervention "
                    "policy covariance matrix column {} name ({})",
                    i, key, i, policy_column_name)};
}

// todo: implement the rest of the parameters
// std::vector<core::DoubleInterval> policy_ranges;
// Eigen::MatrixXd policy_cholesky;
// double info_speed;
// std::unordered_map<core::Identifier;
// std::unordered_map<core::Gender, double>> rural_prevalence;
// std::unordered_map<core::Income;
// LinearModelParams> income_models;
// double physical_activity_stddev;

// Create test fixture for a StaticLinearModel class instance
class StaticLinearModelTestFixture : public::testing::Test {
    public:
        StaticLinearModel testModel(
            expected,
            names,
            models,
            lambda,
            stddev,
            cholesky,
            policy_models,
            policy_ranges,
            policy_cholesky,
            info_speed,
            rural_prevalence,
            income_models,
            physical_activity_stddev
        );
};
