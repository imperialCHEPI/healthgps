#pragma once

#include "HealthGPS/dynamic_hierarchical_linear_model.h"
#include "HealthGPS/kevin_hall_model.h"
#include "HealthGPS/risk_factor_adjustable_model.h"
#include "HealthGPS/static_hierarchical_linear_model.h"
#include "HealthGPS/static_linear_model.h"

#include "configuration.h"
#include "jsonparser.h"

#include <utility>

// TODO: temp include while we port baseline adjustments
#include "HealthGPS/riskfactor_adjustment.h"

namespace host {

/// @brief Loads baseline adjustments information from a file
/// @param info Baseline file information
/// @return An instance of the hgps::BaselineAdjustment type
hgps::BaselineAdjustment load_baseline_adjustments(const poco::BaselineInfo &info);

/// @brief Loads a static risk factor model from a JSON file
/// @param model_name The name of the model to use
/// @param opt The parsed model definition JSON file
/// @param config The model configuration
/// @return An instance of the hgps::RiskFactorModelDefinition type
/// @throw std::invalid_argument if static model is unrecognised
std::unique_ptr<hgps::RiskFactorModelDefinition>
load_static_risk_model_definition(const std::string &model_name, const poco::json &opt,
                                  const host::Configuration &config);

/// @brief Loads the full hierarchical linear regression model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @return An instance of the hgps::StaticHierarchicalLinearModelDefinition type
/// @throw std::invalid_argument if static model is unrecognised
std::unique_ptr<hgps::StaticHierarchicalLinearModelDefinition>
load_hlm_risk_model_definition(const poco::json &opt);

/// @brief Loads the static linear regression model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @param config The model configuration
/// @return An instance of the hgps::StaticLinearModelDefinition type
std::unique_ptr<hgps::StaticLinearModelDefinition>
load_staticlinear_risk_model_definition(const poco::json &opt, const host::Configuration &config);

/// @brief Loads a dynamic risk factor model from a JSON file
/// @param model_name The name of the model to use
/// @param opt The parsed model definition JSON file
/// @param config The model configuration
/// @return An instance of the hgps::RiskFactorModelDefinition type
/// @throw std::invalid_argument if dynamic model is unrecognised
std::unique_ptr<hgps::RiskFactorModelDefinition>
load_dynamic_risk_model_definition(const std::string &model_name, const poco::json &opt,
                                   const host::Configuration &config);

/// @brief Loads the old energy balance model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @return An instance of the hgps::DynamicHierarchicalLinearModelDefinition type
std::unique_ptr<hgps::DynamicHierarchicalLinearModelDefinition>
load_ebhlm_risk_model_definition(const poco::json &opt);

/// @brief Loads the Kevin Hall energy balance model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @param config The model configuration
/// @return An instance of the hgps::KevinHallModelDefinition type
std::unique_ptr<hgps::KevinHallModelDefinition>
load_kevinhall_risk_model_definition(const poco::json &opt, const host::Configuration &config);

/// @brief Loads a risk model definition from a JSON file
/// @param model_type The type of model ("dynamic"/"static") to load
/// @param opt The parsed model definition JSON file
/// @param config The model configuration
/// @return A std::pair containing the model type and model definition
std::pair<hgps::RiskFactorModelType, std::unique_ptr<hgps::RiskFactorModelDefinition>>
load_risk_model_definition(const std::string &model_type, const poco::json &opt,
                           const host::Configuration &config);

/// @brief Load and parse a JSON model file
/// @param model_path The path to the model file
/// @return The parsed JSON
/// @throw std::invalid_argument if file is missing
poco::json load_json(const std::filesystem::path &model_path);

/// @brief Registers a risk factor model definition with the repository
/// @param repository The repository instance to register
/// @param config The model configuration
void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const host::Configuration &config);

} // namespace host
