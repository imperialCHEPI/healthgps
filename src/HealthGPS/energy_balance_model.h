#pragma once

#include "interfaces.h"
#include "mapping.h"

namespace hgps {

/// @brief Result data type for a simulated person
struct SimulatePersonResult {
    /// @brief Height
    double H{};

    /// @brief Body weight
    double BW{};

    /// @brief Pyhsical activity level
    double PAL{};

    /// @brief Body fat
    double F{};

    /// @brief Lean tissue
    double L{};

    /// @brief Extracellular fluid
    double ECF{};

    /// @brief Energy intake
    double EI{};

    /// @brief Energy expenditure
    double EE{};

    /// @brief Baseline adjustment coefficient
    double adjust{};
};

/// @brief Implements the energy balance model type
///
/// @details The dynamic model is used to advance the virtual population over time.
class EnergyBalanceModel final : public HierarchicalLinearModel {
  public:
    /// @brief Initialises a new instance of the EnergyBalanceModel class
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param age_mean_height The mean height at all ages (male and female)
    EnergyBalanceModel(
        const std::unordered_map<core::Identifier, double> &energy_equation,
        const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
            &nutrient_equations,
        const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height);

    HierarchicalModelType type() const noexcept override;

    std::string name() const noexcept override;

    /// @copydoc HierarchicalLinearModel::generate_risk_factors
    /// @throws std::logic_error the dynamic model does not generate risk factors.
    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    const std::unordered_map<core::Identifier, double> &energy_equation_;
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations_;
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
    /// @param final_run Final run or trial run
    /// @param final_shift Offset applied in final run
    /// @return Body weight and baseline adjustment coefficient (if final_run == false)
    SimulatePersonResult simulate_person(Person &person, double shift) const;

    /// @brief Compute new nutrient intakes from food intakes.
    /// @param person The person to compute nutrient intakes for
    /// @return A map of computed nutrient intakes
    std::unordered_map<core::Identifier, double>
    compute_nutrient_intakes(const Person &person) const;

    /// @brief Compute new energy intake from nutrient intakes.
    /// @param nutrient_intakes The nutrient intake
    /// @return The computed energy intake
    double compute_energy_intake(
        const std::unordered_map<core::Identifier, double> &nutrient_intakes) const;

    /// @brief Compute new glycogen.
    /// @param CI The new carbohydrate intake
    /// @param CI_0 The initial carbohydrate intake
    /// @param G_0 The initial glycogen
    /// @return The computed glycogen
    double compute_glycogen(double CI, double CI_0, double G_0) const;

    /// @brief Compute new extracellular fluid.
    /// @param EI The new energy intake
    /// @param EI_0 The initial energy intake
    /// @param CI The new carbohydrate intake
    /// @param CI_0 The initial carbohydrate intake
    /// @param ECF_0 The initial extracellular fluid
    /// @return The computed extracellular fluid
    double compute_extracellular_fluid(double EI, double EI_0, double CI, double CI_0,
                                       double ECF_0) const;
};

/// @brief Defines the energy balance model data type
class EnergyBalanceModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the EnergyBalanceModelDefinition class
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param age_mean_height The mean height at all ages (male and female)
    /// @throws std::invalid_argument for empty arguments
    EnergyBalanceModelDefinition(
        std::unordered_map<core::Identifier, double> energy_equation,
        std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
        std::unordered_map<core::Gender, std::vector<double>> age_mean_height);

    /// @brief Construct a new EnergyBalanceModel from this definition
    /// @return A unique pointer to the new EnergyBalanceModel instance
    std::unique_ptr<HierarchicalLinearModel> create_model() const override;

  private:
    std::unordered_map<core::Identifier, double> energy_equation_;
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations_;
    std::unordered_map<core::Gender, std::vector<double>> age_mean_height_;
};

} // namespace hgps
