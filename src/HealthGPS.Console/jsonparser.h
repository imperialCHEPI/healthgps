#pragma once
#include "poco.h"
#include "riskmodel.h"

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

// Settings Information
void to_json(json &j, const SettingsInfo &p);
void from_json(const json &j, SettingsInfo &p);

// SES Model Information
void to_json(json &j, const SESInfo &p);
void from_json(const json &j, SESInfo &p);

// Baseline model information
void to_json(json &j, const BaselineInfo &p);

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

namespace hgps::core {
using json = nlohmann::json;

template <class T> void to_json(json &j, const Interval<T> &interval) {
    j = json::array({interval.lower(), interval.upper()});
}

template <class T> void from_json(const json &j, Interval<T> &interval) {
    const auto vec = j.get<std::vector<T>>();
    if (vec.size() != 2) {
        throw json::type_error::create(302, "Interval arrays must have only two elements", nullptr);
    }

    interval = Interval<T>{vec[0], vec[1]};
}
} // namespace hgps::core

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

template <typename T>
void to_json(nlohmann::json &j, const std::optional<hgps::core::Interval<T>> &p) {
    if (p) {
        j = *p;
    } else {
        // Null interval expressed as empty JSON array
        j = nlohmann::json::array();
    }
}

template <typename T>
void from_json(const nlohmann::json &j, std::optional<hgps::core::Interval<T>> &p) {
    // Treat null JSON values and empty arrays as a null Interval
    if (j.is_null() || (j.is_array() && j.empty())) {
        p = std::nullopt;
    } else {
        p = j.get<hgps::core::Interval<T>>();
    }
}

} // namespace std
