#include "pch.h"

#include "HealthGPS/static_linear_model.h"
#include "HealthGPS/mtrandom.h"

// Create test fixture for a StaticLinearModel class instance
class StaticLinearModelTestFixture : public::testing::Test {
    public:

        hgps::RiskFactorSexAgeTable expected;
        std::vector<hgps::core::Identifier> names;
        std::vector<hgps::LinearModelParams> models;
        std::vector<double> lambda;
        std::vector<double> stddev;
        Eigen::Matrix2d correlation;
        Eigen::MatrixXd cholesky;
        std::vector<hgps::LinearModelParams> policy_models;
        std::vector<hgps::core::DoubleInterval> policy_ranges;
        Eigen::MatrixXd policy_cholesky;

        hgps::StaticLinearModel* test_model;
        hgps::Person* test_person;
        hgps::Random* test_random;

        void SetUp() override {
            auto expected = hgps::RiskFactorSexAgeTable{};
            std::vector<double> factor_means_male = {200.,40.,30.,2000.,1.,1000.,3.,50.};
            std::vector<double> factor_means_female = {200.,40.,30.,2000.,1.,1000.,3.,50.};

            std::unordered_map<hgps::core::Identifier, std::vector<double>> factors_male = {
                {hgps::core::Identifier{"FoodCarbohydrate"}, factor_means_male},
                {hgps::core::Identifier{"FoodFat"}, factor_means_male}
            };

            std::unordered_map<hgps::core::Identifier, std::vector<double>> factors_female = {
                {hgps::core::Identifier{"FoodCarbohydrate"}, factor_means_female},
                {hgps::core::Identifier{"FoodFat"}, factor_means_female}
            };

            expected.emplace_row(hgps::core::Gender::male, factors_male);
            expected.emplace_row(hgps::core::Gender::female, factors_female);

            std::vector<hgps::core::Identifier> names = {"FoodCarbohydrate", "FoodFat"};

            // Define models
            // We will define 2 RiskFactorModels: carb_model and fat_model 
            std::vector<hgps::LinearModelParams> models;
            hgps::LinearModelParams carb_model;
            carb_model.intercept = -0.241505734056409; 
            carb_model.coefficients = {
                {"Gender", 0.00191412437431455},
                {"Age", -0.000545257990453302},
                {"Age2", 3.06434596341924e-6},
                {"Age3", 5.24468215546687e-9},
                {"Sector", 0.0967308121487847},
                {"Income", 0.0810023788842083}
            };

            hgps::LinearModelParams fat_model;
            fat_model.intercept = -0.693521187677;
            fat_model.coefficients = {
                {"Gender", 0.00385126434589752},
                {"Age", -0.00433711587715954},
                {"Age2", 3.11799231385975e-5},
                {"Age3", 3.4291809335544e-8},
                {"Sector", -0.0629531974852436},
                {"Income", 0.373256996730791}
            };

            models.emplace_back(std::move(carb_model));
            models.emplace_back(std::move(fat_model));

            std::vector<double> lambda = {0.262626262626263, 0.141414141414141};
            std::vector<double> stddev = {0.337810256621877, 0.436763527499362};

            Eigen::Matrix2d correlation;
            correlation << 1.0, 0.322065425, 0.322065425,1.0;
            auto cholesky = Eigen::MatrixXd{Eigen::LLT<Eigen::Matrix2d>{correlation}.matrixL()};

            Eigen::MatrixXd policy_covariance;
            policy_covariance << 0.1,0.1,0.1,0.41;
            auto policy_cholesky = Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{policy_covariance}.matrixL()};

            // Define policy models
            std::vector<hgps::LinearModelParams> policy_models;
            std::vector<hgps::core::DoubleInterval> policy_ranges;

            // We need 2 policy models for the 2 risk factor models
            hgps::LinearModelParams carb_policy_model;
            carb_policy_model.intercept = -1.29385215316281;
            carb_policy_model.coefficients = {
                {"Gender", 0.00845121355575458},
                {"Age", 0.000386003599279214},
                {"Sector", 0.0621763530502617},
                {"Income", 0.0560558815729017}
            };
            carb_policy_model.log_coefficients = {
                {"FoodCarbohydrate", 0.577638667323293},
                {"FoodFat", -0.0801300827856402},
                {"FoodProtein", -0.480946893299471},
                {"FoodSodium", -0.0396319735508116}
            };
            policy_ranges.emplace_back(hgps::core::DoubleInterval{-2.31063, 0.37526});

            hgps::LinearModelParams fat_policy_model;
            fat_policy_model.intercept = -0.734121205356728;
            fat_policy_model.coefficients = {
                {"Gender", 0.023422206650121},
                {"Age", 0.000557687923688474},
                {"Sector", 0.00166651997223223},
                {"Income", -0.498755023022772}
            };
            fat_policy_model.log_coefficients = {
                {"FoodCarbohydrate", 0.652615267035516},
                {"FoodFat", 0.743457905132894},
                {"FoodProtein", -1.2648185364167},
                {"FoodSodium", -0.17181077457271}
            };
            policy_ranges.emplace_back(hgps::core::DoubleInterval{-4.87205, -0.10346});

            policy_models.emplace_back(std::move(carb_policy_model));
            policy_models.emplace_back(std::move(fat_policy_model));

            const double info_speed = 0.1;

            std::unordered_map<hgps::core::Identifier, std::unordered_map<hgps::core::Gender, double>> rural_prevalence;
            rural_prevalence["Under18"] = {{hgps::core::Gender::female, 0.65},{hgps::core::Gender::male, 0.65}};
            rural_prevalence["Over18"] = {{hgps::core::Gender::female, 0.6},{hgps::core::Gender::male, 0.6}};

            std::unordered_map<hgps::core::Income, hgps::LinearModelParams> income_models;
            
            hgps::LinearModelParams income_model_low;
            income_model_low.intercept = 0.0;
            income_model_low.coefficients = {};

            hgps::LinearModelParams income_model_middle;
            income_model_middle.intercept = -0.0023671151435328;
            income_model_middle.coefficients = {
                {"Gender", 0.0244784583947992},
                {"Over18", 0.276669591410528},
                {"Sector", -0.491794449118805}
            };

            hgps::LinearModelParams income_model_high;
            income_model_high.intercept = 0.0187952471153142;
            income_model_high.coefficients = {
                {"Gender", 0.02018185354194},
                {"Over18", 0.607099252829797},
                {"Sector", -1.5870837069653}
            };

            income_models.emplace(hgps::core::Income::low, std::move(income_model_low));
            income_models.emplace(hgps::core::Income::middle, std::move(income_model_middle));
            income_models.emplace(hgps::core::Income::high, std::move(income_model_high));

            const double physical_activity_stddev = 0.06;

            test_person = new hgps::Person();
            auto engine = hgps::MTRandom32{123456789};
            test_random = new hgps::Random(engine);

            test_model = new hgps::StaticLinearModel(
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

};

TEST_F(StaticLinearModelTestFixture, InitialiseFactors) {
    // Just check that initialise_factors runs successfully
    test_model->initialise_factors(*test_person, *test_random);
}