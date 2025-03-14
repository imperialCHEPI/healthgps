#pragma once

#include "HealthGPS.Core/exception.h"

#include "interfaces.h"
#include "map2d.h"
#include "mapping.h"
#include "risk_factor_adjustable_model.h"
#include "static_linear_model.h"

#include <optional>
#include <tuple>
#include <vector>

namespace hgps {

/// @brief Defines a table type for Kevin Hall adjustments by sex and age
using KevinHallAdjustmentTable = UnorderedMap2d<core::Gender, int, double>;

/// @brief Implements the energy balance model type
///
/// @details The dynamic model is used to advance the virtual population over time.
class KevinHallModel final : public RiskFactorAdjustableModel {
  public:
    /// @brief Initialises a new instance of the KevinHallModel class
    /// @param expected The risk factor expected values by sex and age
    /// @param expected_trend The expected trend of risk factor values
    /// @param trend_steps The number of time steps to apply the trend
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_ranges The interval boundaries for nutrient values
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param food_prices The unit price for each food group
    /// @param weight_quantiles The weight quantiles (must be sorted)
    /// @param epa_quantiles The Energy / Physical Activity quantiles (must be sorted)
    /// @param height_stddev The height model female/male standard deviations
    /// @param height_slope The height female/male model slopes
    /// @param region_models The region models for the model
    /// @param ethnicity_models The ethnicity models for the model
    /// @param income_models The income models for the model
    /// @param income_continuous_stddev The standard deviation for continuous income
    KevinHallModel(
        std::shared_ptr<RiskFactorSexAgeTable> expected,
        std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
        const std::unordered_map<core::Identifier, double> &energy_equation,
        const std::unordered_map<core::Identifier, core::DoubleInterval> &nutrient_ranges,
        const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
            &nutrient_equations,
        const std::unordered_map<core::Identifier, std::optional<double>> &food_prices,
        const std::unordered_map<core::Gender, std::vector<double>> &weight_quantiles,
        const std::vector<double> &epa_quantiles,
        const std::unordered_map<core::Gender, double> &height_stddev,
        const std::unordered_map<core::Gender, double> &height_slope,
        std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models,
        std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> ethnicity_models,
        std::unordered_map<core::Income, LinearModelParams> income_models,
        double income_continuous_stddev);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

    /// @brief Initialize the continuous income value for a person
    /// @param person The person to initialize continuous income for
    /// @param random The random number generator
    void initialise_income_continuous(Person &person, Random &random) const;

    /// @brief Update the continuous income value for a person
    /// @param person The person to update continuous income for
    /// @param random The random number generator
    void update_income_continuous(Person &person, Random &random) const;

    /// @brief Calculate income quartile thresholds for the entire population
    /// @param population The population to calculate thresholds for
    /// @return A tuple containing the three quartile thresholds (Q1, Q2, Q3)
    std::tuple<double, double, double>
    calculate_income_thresholds(const Population &population) const;

    /// @brief Initialize the income category for a person based on pre-calculated thresholds
    /// @param person The person to initialize income for
    /// @param q1_threshold The first quartile threshold
    /// @param q2_threshold The second quartile threshold
    /// @param q3_threshold The third quartile threshold
    void initialise_income_category(Person &person, double q1_threshold, double q2_threshold,
                                    double q3_threshold) const;

    /// @brief Update the income category for a person
    /// @param context The runtime context
    void update_income_category(RuntimeContext &context) const;

  private:
    /// @brief Handle the update (initialisation) of newborns separately
    /// @param context The runtime context
    void update_newborns(RuntimeContext &context) const;

    /// @brief Handle the update on non-newborns separately
    /// @param context The runtime context
    void update_non_newborns(RuntimeContext &context) const;

    /// @brief Initialise total nutrient intakes from food intakes
    /// @param person The person to initialise
    /// @param random The random number generator
    void initialise_nutrient_intakes(Person &person, Random &random) const;

    /// @brief Update total nutrient intakes from food intakes
    /// @param person The person to update
    void update_nutrient_intakes(Person &person) const;

    /// @brief Compute and set nutrient intakes from food intakes.
    /// @param person The person to compute nutrient intakes for
    void compute_nutrient_intakes(Person &person) const;

    /// @brief Initialise total energy intake from nutrient intakes
    /// @param person The person to initialise
    void initialise_energy_intake(Person &person) const;

    /// @brief Update total energy intake from nutrient intakes
    /// @param person The person to update
    void update_energy_intake(Person &person) const;

    /// @brief Compute and set energy intake from nutrient intakes.
    /// @param person The person to compute energy intake for
    void compute_energy_intake(Person &person) const;

    /// @brief Compute glycogen.
    /// @param CI The carbohydrate intake
    /// @param CI_0 The initial carbohydrate intake
    /// @param G_0 The initial glycogen
    /// @return The computed glycogen
    double compute_G(double CI, double CI_0, double G_0) const;

    /// @brief Compute extracellular fluid.
    /// @param delta_Na The change in dietary sodium
    /// @param CI The carbohydrate intake
    /// @param CI_0 The initial carbohydrate intake
    /// @param ECF_0 The initial extracellular fluid
    /// @return The computed extracellular fluid
    double compute_ECF(double delta_Na, double CI, double CI_0, double ECF_0) const;

    /// @brief Compute energy cost per unit body weight.
    /// @param age The age of the person
    /// @param sex the sex of the person
    /// @param PAL The physical activity level
    /// @param BW The body weight
    /// @param H The height (cm)
    /// @return The computed energy cost per unit body weight
    double compute_delta(int age, core::Gender sex, double PAL, double BW, double H) const;

    /// @brief Compute energy expenditure.
    /// @param BW The body weight
    /// @param F The fat mass
    /// @param L The lean tissue
    /// @param EI The energy intake
    /// @param K The model intercept
    /// @param delta The energy cost per unit body weight
    /// @param x TODO: what is this?
    /// @return The computed energy expenditure
    double compute_EE(double BW, double F, double L, double EI, double K, double delta,
                      double x) const;

    /// @brief Compute's a person BMI assuming height in cm
    /// @param person The person to calculate the BMI for.
    void compute_bmi(Person &person) const;

    /// @brief Initialises the weight of a person.
    /// @param context The runtime context
    /// @param person The person fo initialise the weight for.
    void initialise_weight(RuntimeContext &context, Person &person) const;

    /// @brief  Initialise the Kevin Hall state variables of a person
    /// @param person The person to initialise
    /// @param adjustment An optional weight adjustment term (default is zero)
    void initialise_kevin_hall_state(Person &person,
                                     std::optional<double> adjustment = std::nullopt) const;

    /// @brief Run the Kevin Hall energy balance model for a given person
    /// @param person The person to simulate
    void kevin_hall_run(Person &person) const;

    /// @brief Adjusts the weight of a person to baseline.
    /// @param person The person fo update the weight for.
    /// @param adjustment The weight adjustment term
    void adjust_weight(Person &person, double adjustment) const;

    /// @brief Computes weight adjustments or receives them from the baseline scenario
    /// @param context The runtime context
    /// @return The computed (baseline) or received (intervention) weight adjustments
    KevinHallAdjustmentTable receive_weight_adjustments(RuntimeContext &context) const;

    /// @brief Sends weight adjustments to the intervention scenario
    /// @param context The runtime context
    /// @param adjustments The weight adjustments to send
    void send_weight_adjustments(RuntimeContext &context,
                                 KevinHallAdjustmentTable &&adjustments) const;

    /// @brief Compute weight adjustments for sex and age
    /// @param context The runtime context
    /// @param age The (optional) age to compute the adjustments for (default all)
    /// @return The weight adjustments by sex and age
    KevinHallAdjustmentTable
    compute_weight_adjustments(RuntimeContext &context,
                               std::optional<unsigned> age = std::nullopt) const;

    /// @brief Gets a person's expected risk factor value
    /// @param context The simulation run-time context
    /// @param sex The sex key to get the expected value
    /// @param age The age key to get the expected value
    /// @param factor The risk factor to get the expected value
    /// @param range An optional expected value range
    /// @param apply_trend Whether to apply expected value time trend
    /// @returns The person's expected risk factor value
    double get_expected(RuntimeContext &context, core::Gender sex, int age,
                        const core::Identifier &factor, OptionalRange range,
                        bool apply_trend) const noexcept override;

    /// @brief Returns the weight quantile for the given E overPA quantile and sex.
    /// @param epa_quantile The Energy / Physical Activity quantile.
    /// @param sex The sex of the person.
    double get_weight_quantile(double epa_quantile, core::Gender sex) const;

    /// @brief Compute the mean of weight (optionally raised to a power) for each sex and age
    /// @param population The population to compute the mean for
    /// @param power The (optional) power to raise the weight to
    /// @param age The (optional) age to compute the mean for (default all)
    /// @return The weight power means by sex and age
    KevinHallAdjustmentTable compute_mean_weight(
        Population &population,
        std::optional<std::unordered_map<core::Gender, double>> power = std::nullopt,
        std::optional<unsigned> age = std::nullopt) const;

    /// @brief Initialises the height of a person.
    /// @param context The runtime context
    /// @param person The person fo initialise the height for.
    /// @param W_power_mean The mean height power for the person's sex and age
    /// @param random The random number generator
    void initialise_height(RuntimeContext &context, Person &person, double W_power_mean,
                           Random &random) const;

    /// @brief Updates the height of a person.
    /// @param context The runtime context
    /// @param person The person fo update the height for.
    /// @param W_power_mean The mean height power for the person's sex and age
    void update_height(RuntimeContext &context, Person &person, double W_power_mean) const;

    /// @brief Initialises the ethnicity of a person.
    /// @param context The runtime context
    /// @param person The person to initialise the ethnicity for
    /// @param random The random number generator
    void initialise_ethnicity(RuntimeContext &context, Person &person, Random &random) const;

    /// @brief Initialises the region of a person.
    /// @param context The runtime context
    /// @param person The person to initialise the region for
    /// @param random The random number generator
    void initialise_region(RuntimeContext &context, Person &person, Random &random) const;

    /// @brief Updates the region of a person.
    /// @param context The runtime context
    /// @param person The person to update the region for
    /// @param random The random number generator
    void update_region(RuntimeContext &context, Person &person, Random &random) const;

    /// @brief Initialize physical activity level for a person
    /// @param context The runtime context
    /// @param person The person to initialize PA for
    /// @param random The random number generator
    void initialise_physical_activity(RuntimeContext &context, Person &person,
                                      Random &random) const;

    // Add after existing member variables
    const std::unordered_map<core::Identifier, double> &energy_equation_;
    const std::unordered_map<core::Identifier, core::DoubleInterval> &nutrient_ranges_;
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations_;
    const std::unordered_map<core::Identifier, std::optional<double>> &food_prices_;
    const std::unordered_map<core::Gender, std::vector<double>> &weight_quantiles_;
    const std::vector<double> &epa_quantiles_;
    const std::unordered_map<core::Gender, double> &height_stddev_;
    const std::unordered_map<core::Gender, double> &height_slope_;
    std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models_;
    std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> ethnicity_models_;
    std::unordered_map<core::Income, LinearModelParams> income_models_;

    // Model parameters.
    static constexpr int kevin_hall_age_min = 19; // Start age for the main Kevin Hall model.
    static constexpr double rho_F = 39.5e3;       // Energy content of fat (kJ/kg).
    static constexpr double rho_L = 7.6e3;        // Energy content of lean (kJ/kg).
    static constexpr double rho_G = 17.6e3;       // Energy content of glycogen (kJ/kg).
    static constexpr double gamma_F = 13.0;       // RMR fat coefficients (kJ/kg/day).
    static constexpr double gamma_L = 92.0;       // RMR lean coefficients (kJ/kg/day).
    static constexpr double eta_F = 750.0;        // Fat synthesis energy coefficient (kJ/kg).
    static constexpr double eta_L = 960.0;        // Lean synthesis energy coefficient (kJ/kg).
    static constexpr double beta_TEF = 0.1;       // TEF from energy intake (unitless).
    static constexpr double beta_AT = 0.14;       // AT from energy intake (unitless).
    static constexpr double xi_Na = 3000.0;       // Na from ECF changes (mg/L/day).
    static constexpr double xi_CI = 4000.0;       // Na from carbohydrate changes (mg/day).

    // Member variables in correct initialization order
    double income_continuous_stddev_;
    double physical_activity_stddev_ = 0.5;
};

/// @brief Defines the energy balance model data type
class KevinHallModelDefinition final : public RiskFactorAdjustableModelDefinition {
  public:
    /// @brief Initialises a new instance of the KevinHallModelDefinition class
    /// @param expected The risk factor expected values by sex and age
    /// @param expected_trend The expected trend of risk factor values
    /// @param trend_steps The number of time steps to apply the trend
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_ranges The interval boundaries for nutrient values
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param food_prices The unit price for each food group
    /// @param weight_quantiles The weight quantiles (must be sorted)
    /// @param epa_quantiles The Energy / Physical Activity quantiles (must be sorted)
    /// @param height_stddev The height model female/male standard deviations
    /// @param height_slope The height model female/male slopes
    /// @param region_models The region models for the model
    /// @param ethnicity_models The ethnicity models for the model
    /// @param income_models The income models for the model
    /// @param income_continuous_stddev The standard deviation for continuous income
    /// @throws std::invalid_argument for empty arguments
    KevinHallModelDefinition(
        std::unique_ptr<RiskFactorSexAgeTable> expected,
        std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
        std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
        std::unordered_map<core::Identifier, double> energy_equation,
        std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges,
        std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
        std::unordered_map<core::Identifier, std::optional<double>> food_prices,
        std::unordered_map<core::Gender, std::vector<double>> weight_quantiles,
        std::vector<double> epa_quantiles, std::unordered_map<core::Gender, double> height_stddev,
        std::unordered_map<core::Gender, double> height_slope,
        std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models,
        std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> ethnicity_models,
        std::unordered_map<core::Income, LinearModelParams> income_models,
        double income_continuous_stddev);

    /// @brief Construct a new KevinHallModel from this definition
    /// @return A unique pointer to the new KevinHallModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::unordered_map<core::Identifier, double> energy_equation_;
    std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges_;
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations_;
    std::unordered_map<core::Identifier, std::optional<double>> food_prices_;
    std::unordered_map<core::Gender, std::vector<double>> weight_quantiles_;
    std::vector<double> epa_quantiles_;
    std::unordered_map<core::Gender, double> height_stddev_;
    std::unordered_map<core::Gender, double> height_slope_;
    std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models_;
    std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> ethnicity_models_;
    std::unordered_map<core::Income, LinearModelParams> income_models_;
    double income_continuous_stddev_;
};

} // namespace hgps
