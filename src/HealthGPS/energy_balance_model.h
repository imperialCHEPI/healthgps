#pragma once

#include "interfaces.h"
#include "mapping.h"

#include <optional>

namespace hgps {

/// @brief Implements the energy balance model type
///
/// @details The dynamic model is used to advance the virtual population over time.
class EnergyBalanceModel final : public HierarchicalLinearModel {
  public:
    /// @brief Initialises a new instance of the EnergyBalanceModel class
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_ranges The minimum and maximum nutrient values
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param food_prices The unit price for each food group
    /// @param age_mean_height The mean height at all ages (male and female)
    EnergyBalanceModel(
        const std::unordered_map<core::Identifier, double> &energy_equation,
        const std::unordered_map<core::Identifier, std::pair<double, double>> &nutrient_ranges,
        const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
            &nutrient_equations,
        const std::unordered_map<core::Identifier, std::optional<double>> &food_prices,
        const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height);

    HierarchicalModelType type() const noexcept override;

    std::string name() const noexcept override;

    /// @copydoc HierarchicalLinearModel::generate_risk_factors
    /// @throws std::logic_error the dynamic model does not generate risk factors.
    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    const std::unordered_map<core::Identifier, double> &energy_equation_;
    const std::unordered_map<core::Identifier, std::pair<double, double>> &nutrient_ranges_;
    const std::unordered_map<core::Identifier, std::map<core::Identifier, double>>
        &nutrient_equations_;
    const std::unordered_map<core::Identifier, std::optional<double>> &food_prices_;
    const std::unordered_map<core::Gender, std::vector<double>> &age_mean_height_;

    /// @brief Return the nutrient value bounded within its range
    /// @param nutrient The nutrient Identifier
    /// @param value The nutrient value to bound
    /// @return The bounded nutrient value
    double bounded_nutrient_value(const core::Identifier &nutrient, double value) const;

    std::map<core::Identifier, double> get_current_risk_factors(const HierarchicalMapping &mapping,
                                                                Person &entity,
                                                                int time_year) const;
};

/// @brief Defines the energy balance model data type
class EnergyBalanceModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the EnergyBalanceModelDefinition class
    /// @param energy_equation The energy coefficients for each nutrient
    /// @param nutrient_ranges The minimum and maximum nutrient values
    /// @param nutrient_equations The nutrient coefficients for each food group
    /// @param food_prices The unit price for each food group
    /// @param age_mean_height The mean height at all ages (male and female)
    /// @throws std::invalid_argument for empty arguments
    EnergyBalanceModelDefinition(
        std::unordered_map<core::Identifier, double> energy_equation,
        std::unordered_map<core::Identifier, std::pair<double, double>> nutrient_ranges,
        std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations,
        std::unordered_map<core::Identifier, std::optional<double>> food_prices,
        std::unordered_map<core::Gender, std::vector<double>> age_mean_height);

    /// @brief Construct a new EnergyBalanceModel from this definition
    /// @return A unique pointer to the new EnergyBalanceModel instance
    std::unique_ptr<HierarchicalLinearModel> create_model() const override;

  private:
    std::unordered_map<core::Identifier, double> energy_equation_;
    std::unordered_map<core::Identifier, std::pair<double, double>> nutrient_ranges_;
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations_;
    std::unordered_map<core::Identifier, std::optional<double>> food_prices_;
    std::unordered_map<core::Gender, std::vector<double>> age_mean_height_;
};

} // namespace hgps
