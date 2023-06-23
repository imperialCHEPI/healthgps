#pragma once

#include "interfaces.h"
#include "mapping.h"

namespace hgps {

/// @brief Implements the energy balance model data type
class EnergyBalanceModelDefinition final {
  public:
    EnergyBalanceModelDefinition() = delete;

    /// @brief Initialises a new instance of the EnergyBalanceModelDefinition class
    /// @param nutrient_coefficients The food group -> nutrient weights
    /// @throws std::invalid_argument if nutrient coefficients map is empty
    EnergyBalanceModelDefinition(
        std::vector<core::Identifier> &&nutrient_list,
        std::map<core::Identifier, std::map<core::Identifier, double>> &&nutrient_equations);

    /// @brief Gets the list of nutrients used in the model
    /// @return A vector of nutrient identities
    const std::vector<core::Identifier> &nutrient_list() const noexcept;

    /// @brief Gets the nutrient coefficients for each food group
    /// @return A map of nutrient coefficients for each food group
    const std::map<core::Identifier, std::map<core::Identifier, double>> &
    nutrient_equations() const noexcept;

  private:
    std::vector<core::Identifier> nutrient_list_;
    std::map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations_;
};

/// @brief Implements the energy balance model type
///
/// @details The dynamic model is used to advance the virtual population over time.
class EnergyBalanceModel final : public HierarchicalLinearModel {
  public:
    /// @brief Initialises a new instance of the EnergyBalanceModel class
    /// @param definition The model definition instance
    EnergyBalanceModel(EnergyBalanceModelDefinition &definition);

    HierarchicalModelType type() const noexcept override;

    const std::string &name() const noexcept override;

    /// @copydoc HierarchicalLinearModel::generate_risk_factors
    /// @throws std::logic_error the dynamic model does not generate risk factors.
    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    std::reference_wrapper<EnergyBalanceModelDefinition> definition_;
    std::string name_{"Dynamic"};

    std::map<core::Identifier, double> get_current_risk_factors(const HierarchicalMapping &mapping,
                                                                Person &entity,
                                                                int time_year) const;
};

} // namespace hgps
