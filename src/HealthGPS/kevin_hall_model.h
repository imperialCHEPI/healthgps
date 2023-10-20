#pragma once

#include "interfaces.h"
#include "mapping.h"

#include <optional>

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

    /// @brief Water
    double W{};

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
class KevinHallModel final : public RiskFactorModel {
  public:
    /// @brief Initialises a new instance of the KevinHallModel class
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_ranges The minimum and maximum nutrient values
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param food_prices The unit price for each food group
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param age_mean_height The mean height at all ages (male and female)
    KevinHallModel(
        const std::unordered_map<core::Identifier, double> &energy_equation,
        const std::unordered_map<core::Identifier, std::pair<double, double>> &nutrient_ranges,
        const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
            &nutrient_equations,
        const std::unordered_map<core::Identifier, std::optional<double>> &food_prices,
        const std::map<hgps::core::IntegerInterval, std::unordered_map<hgps::core::Gender, double>>
            &rural_prevalence,
        const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height);

    RiskFactorModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    const std::unordered_map<core::Identifier, double> &energy_equation_;
    const std::unordered_map<core::Identifier, std::pair<double, double>> &nutrient_ranges_;
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations_;
    const std::unordered_map<core::Identifier, std::optional<double>> &food_prices_;
    const std::map<hgps::core::IntegerInterval, std::unordered_map<hgps::core::Gender, double>>
        &rural_prevalence_;
    const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height_;

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

    /// @brief Simulates the energy balance model for a given person
    /// @param person The person to simulate
    /// @param shift Model adjustment term
    /// @return The state of the person
    SimulatePersonState simulate_person(Person &person, double shift) const;

    /// @brief Compute nutrient intakes from food intakes.
    /// @param person The person to compute nutrient intakes for
    /// @return A map of computed nutrient intakes
    std::unordered_map<core::Identifier, double>
    compute_nutrient_intakes(const Person &person) const;

    /// @brief Compute energy intake from nutrient intakes.
    /// @param nutrient_intakes The nutrient intake
    /// @return The computed energy intake
    double compute_EI(const std::unordered_map<core::Identifier, double> &nutrient_intakes) const;

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

    /// @brief Return the nutrient value bounded within its range
    /// @param nutrient The nutrient Identifier
    /// @param value The nutrient value to bound
    /// @return The bounded nutrient value
    double bounded_nutrient_value(const core::Identifier &nutrient, double value) const;
};

/// @brief Defines the energy balance model data type
class KevinHallModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the KevinHallModelDefinition class
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_ranges The minimum and maximum nutrient values
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param food_prices The unit price for each food group
    /// @param rural_prevalence Rural sector prevalence for age groups and sex
    /// @param age_mean_height The mean height at all ages (male and female)
    /// @throws std::invalid_argument for empty arguments
    KevinHallModelDefinition(
        std::unordered_map<core::Identifier, double> energy_equation,
        std::unordered_map<core::Identifier, std::pair<double, double>> nutrient_ranges,
        std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
        std::unordered_map<core::Identifier, std::optional<double>> food_prices,
        std::map<hgps::core::IntegerInterval, std::unordered_map<hgps::core::Gender, double>>
            rural_prevalence,
        std::unordered_map<core::Gender, std::vector<double>> age_mean_height);

    /// @brief Construct a new KevinHallModel from this definition
    /// @return A unique pointer to the new KevinHallModel instance
    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::unordered_map<core::Identifier, double> energy_equation_;
    std::unordered_map<core::Identifier, std::pair<double, double>> nutrient_ranges_;
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations_;
    std::unordered_map<core::Identifier, std::optional<double>> food_prices_;
    std::map<hgps::core::IntegerInterval, std::unordered_map<hgps::core::Gender, double>>
        rural_prevalence_;
    std::unordered_map<core::Gender, std::vector<double>> age_mean_height_;
};

} // namespace hgps
