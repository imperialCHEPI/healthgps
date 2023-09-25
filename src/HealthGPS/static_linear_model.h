#pragma once

#include "interfaces.h"
#include "mapping.h"

#include "hierarchical_model_static.h"

namespace hgps {

/// @brief Implements the static linear model type
///
/// @details The static model is used to initialise the virtual population.
class StaticLinearModel final : public HierarchicalLinearModel {
  public:
    /// @brief Initialises a new instance of the StaticLinearModel class
    /// @param models The model equations
    /// @param levels The hierarchical model level definition
    StaticLinearModel(const std::unordered_map<core::Identifier, LinearModel> &models,
                      const std::map<int, HierarchicalLevel> &levels);

    HierarchicalModelType type() const noexcept override;

    std::string name() const noexcept override;

    void generate_risk_factors(RuntimeContext &context) override;

    void update_risk_factors(RuntimeContext &context) override;

  private:
    const std::unordered_map<core::Identifier, LinearModel> &models_;
    const std::map<int, HierarchicalLevel> &levels_;

    void generate_for_entity(RuntimeContext &context, Person &entity, int level,
                             std::vector<MappingEntry> &level_factors);
};

/// @brief Defines the static linear model data type
class StaticLinearModelDefinition final : public RiskFactorModelDefinition {
  public:
    /// @brief Initialises a new instance of the StaticLinearModelDefinition class
    /// @param linear_models The linear regression models equations
    /// @param model_levels The hierarchical model levels definition
    /// @throws std::invalid_argument for empty arguments
    StaticLinearModelDefinition(std::unordered_map<core::Identifier, LinearModel> linear_models,
                                std::map<int, HierarchicalLevel> model_levels);

    /// @brief Construct a new StaticLinearModel from this definition
    /// @return A unique pointer to the new StaticLinearModel instance
    std::unique_ptr<HierarchicalLinearModel> create_model() const override;

  private:
    std::unordered_map<core::Identifier, LinearModel> models_;
    std::map<int, HierarchicalLevel> levels_;
};

} // namespace hgps
