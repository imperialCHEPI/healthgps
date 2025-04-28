#pragma once

#include "HealthGPS/dummy_model.h"
#include "HealthGPS/dynamic_hierarchical_linear_model.h"
#include "HealthGPS/kevin_hall_model.h"
#include "HealthGPS/risk_factor_adjustable_model.h"
#include "HealthGPS/static_hierarchical_linear_model.h"
#include "HealthGPS/static_linear_model.h"

#include "configuration.h"
#include "jsonparser.h"

#include <filesystem>
#include <utility>

namespace hgps::input {

/// @brief Loads risk factor expected values from a file
/// @param config The model configuration
/// @return An instance of the hgps::RiskFactorSexAgeTable type
std::unique_ptr<hgps::RiskFactorSexAgeTable> load_risk_factor_expected(const Configuration &config);

/// @brief Loads either a static or dynamic dummy risk factor model from a JSON file
/// @param type Model type (static or dynamic)
/// @param opt The parsed model definition JSON file
/// @return An instance of the hgps::DummyModelDefinition type
/// @throw std::invalid_argument if error parsing model configuration file
std::unique_ptr<hgps::DummyModelDefinition>
load_dummy_risk_model_definition(hgps::RiskFactorModelType type, const nlohmann::json &opt);

/// @brief Loads the full hierarchical linear regression model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @return An instance of the hgps::StaticHierarchicalLinearModelDefinition type
/// @throw std::invalid_argument if static model is unrecognised
std::unique_ptr<hgps::StaticHierarchicalLinearModelDefinition>
load_hlm_risk_model_definition(const nlohmann::json &opt);

/// @brief Loads the static linear regression model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @param config The model configuration
/// @return An instance of the hgps::StaticLinearModelDefinition type
std::unique_ptr<hgps::StaticLinearModelDefinition>
load_staticlinear_risk_model_definition(const nlohmann::json &opt, const Configuration &config);

/// @brief Loads the old energy balance model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @return An instance of the hgps::DynamicHierarchicalLinearModelDefinition type
std::unique_ptr<hgps::DynamicHierarchicalLinearModelDefinition>
load_ebhlm_risk_model_definition(const nlohmann::json &opt, const Configuration &config);

/// @brief Loads the Kevin Hall energy balance model definition from a JSON file
/// @param opt The parsed model definition JSON file
/// @param config The model configuration
/// @return An instance of the hgps::KevinHallModelDefinition type
std::unique_ptr<hgps::KevinHallModelDefinition>
load_kevinhall_risk_model_definition(const nlohmann::json &opt, const Configuration &config);

/// @brief Loads a risk model definition from a JSON file
/// @param model_type The type of model to load
/// @param model_path The path to the model configuration file
/// @param config The model configuration
/// @return The model definition
std::unique_ptr<hgps::RiskFactorModelDefinition>
load_risk_model_definition(hgps::RiskFactorModelType model_type,
                           const std::filesystem::path &model_path, const Configuration &config);

/// @brief Load and parse a JSON model file
/// @param model_path The path to the model file
/// @return The parsed JSON
/// @throw std::invalid_argument if file is missing
nlohmann::json load_json(const std::filesystem::path &filepath);

/// @brief Loads risk factor coefficients from a CSV file
/// @param csv_path The path to the CSV file containing the coefficients
/// @param print_debug Whether to print debug information
/// @return A map from risk factor names to their linear model parameters
std::unordered_map<std::string, hgps::LinearModelParams>
load_risk_factor_coefficients_from_csv(const std::filesystem::path &csv_path, bool print_debug = true);

/// @brief Loads policy ranges from a CSV file
/// @param csv_path The path to the CSV file containing the policy ranges
/// @return A map from risk factor names to their policy ranges
std::unordered_map<std::string, hgps::core::DoubleInterval>
load_policy_ranges_from_csv(const std::filesystem::path &csv_path);

/// @brief Loads logistic regression coefficients from a CSV file
/// @param csv_path The path to the CSV file containing the logistic regression coefficients
/// @param print_debug Whether to print debug information
/// @return A map from risk factor names to their linear model parameters
std::unordered_map<std::string, hgps::LinearModelParams>
load_logistic_regression_coefficients_from_csv(const std::filesystem::path &csv_path, bool print_debug = true);

/// @brief Registers a risk factor model definition with the repository
/// @param repository The repository instance to register
/// @param config The model configuration
void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const Configuration &config);

/// @brief Parses a region from a string
/// @param value The string representation of the region
/// @return The parsed region
core::Region parse_region(const std::string &value);

} // namespace hgps::input
