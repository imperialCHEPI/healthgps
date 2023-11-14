#pragma once

#include "interfaces.h"
#include "map2d.h"
#include "mapping.h"
#include "risk_factor_adjustable_model.h"

#include <optional>
#include <vector>

namespace hgps {

/// @brief State data type for a simulated person
struct SimulatePersonState {
    /// @brief Height
    double H{};

    /// @brief Body weight
    double BW{};

    /// @brief Physical activity level
    double PAL{};

    /// @brief Body fat
    double F{};

    /// @brief Lean tissue
    double L{};

    /// @brief Extracellular fluid
    double ECF{};

    /// @brief Glycogen
    double G{};

    /// @brief Energy expenditure
    double EE{};

    /// @brief Energy intake
    double EI{};

    /// @brief Baseline adjustment coefficient
    double adjust{};
};

/// @brief Implements the energy balance model type
///
/// @details The dynamic model is used to advance the virtual population over time.
class KevinHallModel final : public RiskFactorAdjustableModel {
  public:
    /// @brief Initialises a new instance of the KevinHallModel class
    /// @param expected The risk factor expected values by sex and age
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_ranges The interval boundaries for nutrient values
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param food_prices The unit price for each food group
    /// @param age_mean_height The mean height at all ages (male and female)
    /// @param weight_quantiles The weight quantiles (must be sorted)
    /// @param epa_quantiles The Energy / Physical Activity quantiles (must be sorted)
    KevinHallModel(
        const RiskFactorSexAgeTable &expected,
        const std::unordered_map<core::Identifier, double> &energy_equation,
        const std::unordered_map<core::Identifier, core::DoubleInterval> &nutrient_ranges,
        const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
            &nutrient_equations,
        const std::unordered_map<core::Identifier, std::optional<double>> &food_prices,
        const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height,
        const std::unordered_map<core::Gender, std::vector<double>> &weight_quantiles,
        const std::vector<double> &epa_quantiles);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    /// @brief Initialise total nutrient intakes from food intakes
    /// @param person The person to initialise
    void initialise_nutrient_intakes(Person &person) const;

    /// @brief Update total nutrient intakes from food intakes
    /// @param person The person to update
    void update_nutrient_intakes(Person &person) const;

    /// @brief Compute and set nutrient intakes from food intakes.
    /// @param person The person to compute nutrient intakes for
    void set_nutrient_intakes(Person &person) const;

    /// @brief Initialise total energy intake from nutrient intakes
    /// @param person The person to initialise
    void initialise_energy_intake(Person &person) const;

    /// @brief Update total energy intake from nutrient intakes
    /// @param person The person to update
    void update_energy_intake(Person &person) const;

    /// @brief Compute and set energy intake from nutrient intakes.
    /// @param person The person to compute energy intake for
    void set_energy_intake(Person &person) const;

    /// @brief Simulates the energy balance model for a given person
    /// @param person The person to simulate
    /// @param shift Model adjustment term
    /// @return The state of the person
    SimulatePersonState simulate_person(Person &person, double shift) const;

    /// @brief Compute glycogen.
    /// @param CI The carbohydrate intake
    /// @param CI_0 The initial carbohydrate intake
    /// @param G_0 The initial glycogen
    /// @return The computed glycogen
    double compute_G(double CI, double CI_0, double G_0) const;

    /// @brief Compute water.
    /// @param G The glycogen
    /// @return The computed water
    double compute_W(double G) const;

    /// @brief Compute extracellular fluid.
    /// @param EI The energy intake
    /// @param EI_0 The initial energy intake
    /// @param CI The carbohydrate intake
    /// @param CI_0 The initial carbohydrate intake
    /// @param ECF_0 The initial extracellular fluid
    /// @return The computed extracellular fluid
    double compute_ECF(double EI, double EI_0, double CI, double CI_0, double ECF_0) const;

    /// @brief Compute energy cost per unit body weight.
    /// @param PAL The physical activity level
    /// @param RMR The resting metabolic rate
    /// @param BW The body weight
    /// @return The computed energy cost per unit body weight
    double compute_delta(double PAL, double BW, double H, unsigned int age,
                         core::Gender gender) const;

    /// @brief Compute thermic effect of food.
    /// @param EI The energy intake
    /// @param EI_0 The initial energy intake
    /// @return The computed thermic effect of food
    double compute_TEF(double EI, double EI_0) const;

    /// @brief Compute adaptive thermogenesis.
    /// @param EI The energy intake
    /// @param EI_0 The initial energy intake
    /// @return The computed adaptive thermogenesis
    double compute_AT(double EI, double EI_0) const;

    /// @brief Initialises the weight of a person.
    /// @param person The person fo initialise the weight for.
    void initialise_weight(Person &person) const;

    /// @brief Returns the weight quantile for the given E overPA quantile and sex.
    /// @param epa_quantile The Energy / Physical Activity quantile.
    /// @param sex The sex of the person.
    double get_weight_quantile(double epa_quantile, core::Gender sex) const;

    /// @brief Compute the mean of weight raised to power for eacg sex and age
    /// @param population The population to compute the mean for
    /// @param power The power to raise the weight to
    /// @return The weight power means by sex and age
    UnorderedMap2d<core::Gender, int, double> compute_mean_weight_powers(Population &population,
                                                                         double power) const;

    // // TODO: implement this
    // /// @brief Initialises the height of a person.
    // /// @param person The person fo initialise the height for.
    // void initialise_height(Person &person);

    // // TODO: implement this
    // /// @brief Updates the height of a person.
    // /// @param person The person fo update the height for.
    // void update_height(Person &person);

    // // TODO: implement this
    // /// @brief Compute a new height value for the given person.
    // /// @param person The person to compute the height for.
    // /// @return The computed height.
    // double compute_new_height(Person &person);

    const std::unordered_map<core::Identifier, double> &energy_equation_;
    const std::unordered_map<core::Identifier, core::DoubleInterval> &nutrient_ranges_;
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations_;
    const std::unordered_map<core::Identifier, std::optional<double>> &food_prices_;
    const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height_;
    const std::unordered_map<core::Gender, std::vector<double>> &weight_quantiles_;
    const std::vector<double> &epa_quantiles_;

    // Model parameters.
    static constexpr double rho_F = 39.5e3; // Energy content of fat (kJ/kg).
    static constexpr double rho_L = 7.6e3;  // Energy content of lean (kJ/kg).
    static constexpr double gamma_F = 13.0; // RMR fat coefficients (kJ/kg/day).
    static constexpr double gamma_L = 92.0; // RMR lean coefficients (kJ/kg/day).
    static constexpr double eta_F = 750.0;  // Fat synthesis energy coefficient (kJ/kg).
    static constexpr double eta_L = 960.0;  // Lean synthesis energy coefficient (kJ/kg).
    static constexpr double beta_TEF = 0.1; // TEF from energy intake (unitless).
    static constexpr double beta_AT = 0.14; // AT from energy intake (unitless).
    static constexpr double xi_Na = 3000.0; // Na from ECF changes (mg/L/day).
    static constexpr double xi_CI = 4000.0; // Na from carbohydrate changes (mg/day).
};

/// @brief Defines the energy balance model data type
class KevinHallModelDefinition final : public RiskFactorAdjustableModelDefinition {
  public:
    /// @brief Initialises a new instance of the KevinHallModelDefinition class
    /// @param expected The risk factor expected values by sex and age
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_ranges The interval boundaries for nutrient values
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param food_prices The unit price for each food group
    /// @param age_mean_height The mean height at all ages (male and female)
    /// @param weight_quantiles The weight quantiles (must be sorted)
    /// @param epa_quantiles The Energy / Physical Activity quantiles (must be sorted)
    /// @throws std::invalid_argument for empty arguments
    KevinHallModelDefinition(
        RiskFactorSexAgeTable expected,
        std::unordered_map<core::Identifier, double> energy_equation,
        std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges,
        std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
        std::unordered_map<core::Identifier, std::optional<double>> food_prices,
        std::unordered_map<core::Gender, std::vector<double>> age_mean_height,
        std::unordered_map<core::Gender, std::vector<double>> weight_quantiles,
        std::vector<double> epa_quantiles);

    /// @brief Construct a new KevinHallModel from this definition
    /// @return A unique pointer to the new KevinHallModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::unordered_map<core::Identifier, double> energy_equation_;
    std::unordered_map<core::Identifier, core::DoubleInterval> nutrient_ranges_;
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations_;
    std::unordered_map<core::Identifier, std::optional<double>> food_prices_;
    std::unordered_map<core::Gender, std::vector<double>> age_mean_height_;
    std::unordered_map<core::Gender, std::vector<double>> weight_quantiles_;
    std::vector<double> epa_quantiles_;
};

} // namespace hgps
