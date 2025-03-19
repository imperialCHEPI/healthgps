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

/// @brief Registers a risk factor model definition with the repository
/// @param repository The repository instance to register
/// @param config The model configuration
void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const Configuration &config);

/// @brief Parses a region from a string
/// @param value The string representation of the region
/// @return The parsed region
core::Region parse_region(const std::string &value);

/// @brief Parses an ethnicity from a string
/// @param value The string representation of the ethnicity
/// @return The parsed ethnicity
core::Ethnicity parse_ethnicity(const std::string &value);

// Helper functions for loading static linear model definition
namespace detail {
// Processes risk factor models from JSON and populates data structures
void process_risk_factor_models(
    const nlohmann::json &models_json, const core::DataTable &correlation_table,
    const core::DataTable &policy_covariance_table, std::vector<core::Identifier> &names,
    std::vector<LinearModelParams> &models, std::vector<core::DoubleInterval> &ranges,
    std::vector<double> &lambda, std::vector<double> &stddev,
    std::vector<LinearModelParams> &policy_models, std::vector<core::DoubleInterval> &policy_ranges,
    std::unique_ptr<std::vector<LinearModelParams>> &trend_models,
    std::unique_ptr<std::vector<core::DoubleInterval>> &trend_ranges,
    std::unique_ptr<std::vector<double>> &trend_lambda,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> &expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> &expected_trend_boxcox,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> &trend_steps,
    Eigen::MatrixXd &correlation, Eigen::MatrixXd &policy_covariance);

// Processes income models from JSON
std::unordered_map<core::Income, LinearModelParams>
process_income_models(const nlohmann::json &income_models_json);

// Processes region models from JSON
std::unordered_map<core::Region, LinearModelParams>
process_region_models(const nlohmann::json &region_models_json);

// Processes ethnicity models from JSON
std::unordered_map<core::Ethnicity, LinearModelParams>
process_ethnicity_models(const nlohmann::json &ethnicity_models_json);

// Processes rural prevalence data from JSON
std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
process_rural_prevalence(const nlohmann::json &rural_prevalence_json);

// Validates that expected values exist for all risk factors
void validate_expected_values(const std::vector<core::Identifier> &names,
                              const std::unique_ptr<hgps::RiskFactorSexAgeTable> &expected);
} // namespace detail

} // namespace hgps::input
