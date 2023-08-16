#pragma once
#include "HealthGPS.Core/interval.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Data structures containing model parameters and configuration options
 *
 * POCO stands for "plain old class object". These structs represent data structures
 * which are contained in JSON-formatted configuration files.
 */
namespace host::poco {
//! Information about a data file to be loaded
struct FileInfo {
    std::filesystem::path name;
    std::string format;
    std::string delimiter;
    std::map<std::string, std::string> columns;

    auto operator<=>(const FileInfo &rhs) const = default;
};

//! Experiment's population settings
struct SettingsInfo {
    std::string country;
    hgps::core::IntegerInterval age_range;
    float size_fraction{};

    auto operator<=>(const SettingsInfo &rhs) const = default;
};

//! Socio-economic status (SES) model inputs
struct SESInfo {
    std::string function;
    std::vector<double> parameters;

    auto operator<=>(const SESInfo &) const = default;
};

//! Baseline adjustment information
struct BaselineInfo {
    std::string format;
    std::string delimiter;
    std::string encoding;
    std::map<std::string, std::filesystem::path> file_names;

    auto operator<=>(const BaselineInfo &rhs) const = default;
};

//! Information about a health risk factor
struct RiskFactorInfo {
    std::string name;
    int level{};
    std::optional<hgps::core::DoubleInterval> range;

    auto operator<=>(const RiskFactorInfo &rhs) const = default;
};

//! User-defined model and parameter information
struct ModellingInfo {
    std::vector<RiskFactorInfo> risk_factors;
    std::unordered_map<std::string, std::filesystem::path> risk_factor_models;
    BaselineInfo baseline_adjustment;
};

//! Experiment output folder and file information
struct OutputInfo {
    unsigned int comorbidities{};
    std::string folder{};
    std::string file_name{};

    auto operator<=>(const OutputInfo &rhs) const = default;
};

//! Information about the period over which a policy is applied
struct PolicyPeriodInfo {
    int start_time{};
    std::optional<int> finish_time;

    std::string to_finish_time_str() const {
        if (finish_time.has_value()) {
            return std::to_string(finish_time.value());
        }

        return "null";
    }

    auto operator<=>(const PolicyPeriodInfo &rhs) const = default;
};

//! Information about policy impacts
struct PolicyImpactInfo {
    std::string risk_factor{};
    double impact_value{};
    unsigned int from_age{};
    std::optional<unsigned int> to_age{};
    std::string to_age_str() const {
        if (to_age.has_value()) {
            return std::to_string(to_age.value());
        }

        return "null";
    }

    auto operator<=>(const PolicyImpactInfo &rhs) const = default;
};

//! Extra adjustments made to a policy
struct PolicyAdjustmentInfo {
    std::string risk_factor{};
    double value{};

    auto operator<=>(const PolicyAdjustmentInfo &rhs) const = default;
};

//! Information about an active policy intervention
struct PolicyScenarioInfo {
    std::string identifier{};
    PolicyPeriodInfo active_period;
    std::vector<double> dynamics;
    std::vector<double> coefficients;
    std::vector<double> coverage_rates;
    std::optional<unsigned int> coverage_cutoff_time{};
    std::optional<unsigned int> child_cutoff_age{};
    std::vector<PolicyAdjustmentInfo> adjustments;
    std::string impact_type{};
    std::vector<PolicyImpactInfo> impacts;

    std::string to_coverage_cutoff_time_str() const {
        if (coverage_cutoff_time.has_value()) {
            return std::to_string(coverage_cutoff_time.value());
        }

        return "null";
    }

    std::string to_child_cutoff_age_str() const {
        if (child_cutoff_age.has_value()) {
            return std::to_string(child_cutoff_age.value());
        }

        return "null";
    }

    auto operator<=>(const PolicyScenarioInfo &rhs) const = default;
};
} // namespace host::poco