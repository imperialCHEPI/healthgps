#pragma once
#include "options.h"
#include "riskmodel.h"

#include <nlohmann/json.hpp>

namespace host::poco {
/// @brief JSON parser namespace alias.
///
/// Configuration file serialisation / de-serialisation mapping specific
/// to the `JSON for Modern C++` library adopted by the project.
///
/// @sa https://github.com/nlohmann/json#arbitrary-types-conversions
/// for details about the contents and code structure in this file.
using json = nlohmann::json;

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

// Data file information
void to_json(json &j, const FileInfo &p);
void from_json(const json &j, FileInfo &p);

// Settings Information
void to_json(json &j, const SettingsInfo &p);
void from_json(const json &j, SettingsInfo &p);

// SES Model Information
void to_json(json &j, const SESInfo &p);
void from_json(const json &j, SESInfo &p);

// Baseline scenario adjustments
void to_json(json &j, const BaselineInfo &p);
void from_json(const json &j, BaselineInfo &p);

// Lite risk factors models (Energy Balance Model)
void to_json(json &j, const RiskFactorInfo &p);
void from_json(const json &j, RiskFactorInfo &p);

void to_json(json &j, const VariableInfo &p);
void from_json(const json &j, VariableInfo &p);

void to_json(json &j, const ModellingInfo &p);
void from_json(const json &j, ModellingInfo &p);

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
