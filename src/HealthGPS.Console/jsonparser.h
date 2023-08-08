#pragma once
#include "options.h"
#include "riskmodel.h"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>

namespace host::poco {
/// @brief JSON parser namespace alias.
///
/// Configuration file serialisation / de-serialisation mapping specific
/// to the `JSON for Modern C++` library adopted by the project.
///
/// @sa https://github.com/nlohmann/json#arbitrary-types-conversions
/// for details about the contents and code structure in this file.
using json = nlohmann::json;

/// @brief Get a path, based on base_dir, and check if it exists
/// @param j Input JSON
/// @param base_dir Base folder
/// @return An absolute path, assuming that base_dir is the base if relative
/// @throw json::type_error: Invalid JSON types
/// @throw std::invalid_argument: Path does not exist
std::filesystem::path get_valid_path(const json &j, const std::filesystem::path &base_dir);

/// @brief Load FileInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @return FileInfo
/// @throw json::type_error: Invalid JSON types
/// @throw std::invalid_argument: Path does not exist
FileInfo get_file_info(const json &j, const std::filesystem::path &base_dir);

/// @brief Load BaselineInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @return BaselineInfo
/// @throw json::type_error: Invalid JSON types
/// @throw std::invalid_argument: Path does not exist
BaselineInfo get_baseline_info(const json &j, const std::filesystem::path &base_dir);

/// @brief Load ModellingInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @return ModellingInfo
/// @throw json::type_error: Invalid JSON types
/// @throw std::invalid_argument: Path does not exist
ModellingInfo get_modelling_info(const json &j, const std::filesystem::path &base_dir);

//--------------------------------------------------------
// Full risk factor model POCO types mapping
//--------------------------------------------------------

// Linear models
void to_json(json &j, const CoefficientInfo &p);
void from_json(const json &j, CoefficientInfo &p);

void to_json(json &j, const LinearModelInfo &p);
void from_json(const json &j, LinearModelInfo &p);

// Hierarchical levels
void to_json(json &j, const Array2Info &p);
void from_json(const json &j, Array2Info &p);

void to_json(json &j, const HierarchicalLevelInfo &p);
void from_json(const json &j, HierarchicalLevelInfo &p);

//--------------------------------------------------------
// Configuration sections POCO types mapping
//--------------------------------------------------------

// Settings Information
void to_json(json &j, const SettingsInfo &p);
void from_json(const json &j, SettingsInfo &p);

// SES Model Information
void to_json(json &j, const SESInfo &p);
void from_json(const json &j, SESInfo &p);

// Lite risk factors models (Energy Balance Model)
void from_json(const json &j, RiskFactorInfo &p);

void to_json(json &j, const VariableInfo &p);
void from_json(const json &j, VariableInfo &p);

void to_json(json &j, const FactorDynamicEquationInfo &p);
void from_json(const json &j, FactorDynamicEquationInfo &p);

// Policy Scenario
void to_json(json &j, const PolicyPeriodInfo &p);
void from_json(const json &j, PolicyPeriodInfo &p);

void to_json(json &j, const PolicyImpactInfo &p);
void from_json(const json &j, PolicyImpactInfo &p);

void to_json(json &j, const PolicyAdjustmentInfo &p);
void from_json(const json &j, PolicyAdjustmentInfo &p);

void to_json(json &j, const PolicyScenarioInfo &p);
void from_json(const json &j, PolicyScenarioInfo &p);

// Output information
void to_json(json &j, const OutputInfo &p);
void from_json(const json &j, OutputInfo &p);

} // namespace host::poco

namespace std {

// Optional parameters
template <typename T> void to_json(nlohmann::json &j, const std::optional<T> &p) {
    if (p) {
        j = *p;
    } else {
        j = nullptr;
    }
}
template <typename T> void from_json(const nlohmann::json &j, std::optional<T> &p) {
    if (j.is_null()) {
        p = std::nullopt;
    } else {
        p = j.get<T>();
    }
}

} // namespace std
