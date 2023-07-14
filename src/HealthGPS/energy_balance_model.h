#pragma once

#include "interfaces.h"
#include "mapping.h"

namespace hgps {

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

    void get_steady_state(Person &person, double offset = 0.0);
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
