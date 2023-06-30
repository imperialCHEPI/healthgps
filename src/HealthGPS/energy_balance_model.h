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
    /// @param nutrient_list The list of nutrients
    /// @param nutrient_equations The nutrient equations for each food group
    EnergyBalanceModel(
        const std::vector<core::Identifier> &nutrient_list,
        const std::map<core::Identifier, std::map<core::Identifier, double>> &nutrient_equations);

    HierarchicalModelType type() const noexcept override;

    std::string name() const noexcept override;

    /// @copydoc HierarchicalLinearModel::generate_risk_factors
    /// @throws std::logic_error the dynamic model does not generate risk factors.
    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    std::reference_wrapper<const std::vector<core::Identifier>> nutrient_list_;
    std::reference_wrapper<const std::map<core::Identifier, std::map<core::Identifier, double>>>
        nutrient_equations_;

    std::map<core::Identifier, double> get_current_risk_factors(const HierarchicalMapping &mapping,
                                                                Person &entity,
                                                                int time_year) const;
};

/// @brief Defines the energy balance model data type
class EnergyBalanceModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the EnergyBalanceModelDefinition class
    /// @param nutrient_coefficients The food group -> nutrient weights
    /// @param nutrient_equations The nutrient equations for each food group
    /// @throws std::invalid_argument for empty arguments
    EnergyBalanceModelDefinition(
        std::vector<core::Identifier> nutrient_list,
        std::map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations);

    /// @brief Construct a new EnergyBalanceModel from this definition
    /// @return A unique pointer to the new EnergyBalanceModel instance
    std::unique_ptr<HierarchicalLinearModel> create_model() const override;

  private:
    std::vector<core::Identifier> nutrient_list_;
    std::map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations_;
};

} // namespace hgps
