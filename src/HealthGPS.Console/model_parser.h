#pragma once
#include "HealthGPS/repository.h"
#include "HealthGPS/riskfactor_adjustment_types.h"

#include "jsonparser.h"
#include "options.h"

#include <utility>

namespace host {

/// @brief Loads baseline adjustments information from a file
/// @param info Baseline file information
/// @return An instance of the hgps::BaselineAdjustment type
hgps::BaselineAdjustment load_baseline_adjustments(const poco::BaselineInfo &info);

/// @brief Loads the full hierarchical linear regression model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @return An instance of the hgps::HierarchicalLinearModelDefinition type
std::unique_ptr<hgps::RiskFactorModelDefinition>
load_static_risk_model_definition(const poco::json &opt);

/// @brief Loads a dynamic model from a JSON file
/// @param opt The parsed model definition JSON file
/// @return An instance of the hgps::LiteHierarchicalModelDefinition type
std::unique_ptr<hgps::RiskFactorModelDefinition>
load_dynamic_risk_model_definition(const poco::json &opt);

/// @brief Loads the old energy balance model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @return An instance of the hgps::LiteHierarchicalModelDefinition type
std::unique_ptr<hgps::LiteHierarchicalModelDefinition>
load_ebhlm_risk_model_definition(const poco::json &opt);

/// @brief Loads the new energy balance model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @param settings The main model settings
/// @return An instance of the hgps::EnergyBalanceModelDefinition type
std::unique_ptr<hgps::EnergyBalanceModelDefinition>
load_newebm_risk_model_definition(const poco::json &opt, const poco::SettingsInfo &settings);

/// @brief Loads a risk model definition from a JSON file
/// @param model_type The type of model ("dynamic"/"static") to load
/// @param opt The parsed model definition JSON file
/// @param settings The main model settings
/// @return A std::pair containing the model type and model definition
std::pair<hgps::HierarchicalModelType, std::unique_ptr<hgps::RiskFactorModelDefinition>>
load_risk_model_definition(const std::string &model_type, const poco::json &opt,
                           const poco::SettingsInfo &settings);

/// @brief Load and parse the model file
/// @param model_filename The path to the model
/// @return The parsed JSON
poco::json load_json(const std::string &model_filename);

/// @brief Registers a risk factor model definition with the repository
/// @param repository The repository instance to register
/// @param info The model definition information
/// @param settings The associated experiment settings
void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const poco::ModellingInfo &info,
                                            const poco::SettingsInfo &settings);
} // namespace host
