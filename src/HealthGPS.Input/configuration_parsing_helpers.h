/**
 * @file configuration_parsing_helpers.h
 * @brief This file contains helper functions used by the configuration parsing code
 *
 * The reason for placing these definitions in a separate header is so as to keep the
 * main configuration_parsing.h file tidy, whilst also making them available to testing
 * code.
 */
#pragma once
#include "configuration.h"
#include "poco.h"

#include "HealthGPS.Core/income_category_layout.h"

#include <fmt/color.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

namespace hgps::input {
/// @brief Load value from JSON, printing an error message if it fails
/// @param j JSON object
/// @param key Key to value
/// @throw ConfigurationError: Key not found
/// @return Key value
nlohmann::json get(const nlohmann::json &j, const std::string &key);

/// @brief Get value from JSON object and store in out
/// @tparam T Type of output object
/// @param j JSON object
/// @param key Key to value
/// @param out Output object
/// @return True if value was retrieved successfully, false otherwise
template <class T> bool get_to(const nlohmann::json &j, const std::string &key, T &out) noexcept {
    try {
        out = j.at(key).get<T>();
        return true;
    } catch (const nlohmann::json::out_of_range &) {
        fmt::print(fg(fmt::color::red), "Missing key \"{}\"\n", key);
        return false;
    } catch (const nlohmann::json::type_error &) {
        fmt::print(fg(fmt::color::red), "Key \"{}\" is of wrong type\n", key);
        return false;
    }
}

/// @brief Get value from JSON object and store in out, setting success flag
/// @tparam T Type of output object
/// @param j JSON object
/// @param key Key to value
/// @param out Output object
/// @param success Success flag, set to false in case of failure
/// @return True if value was retrieved successfully, false otherwise
template <class T>
bool get_to(const nlohmann::json &j, const std::string &key, T &out, bool &success) noexcept {
    const bool ret = get_to(j, key, out);
    if (!ret) {
        success = false;
    }
    return ret;
}

/// @brief Rebase path on base_dir
/// @param path Initial path (relative or absolute)
/// @param base_dir New base directory for relative path
/// @throw ConfigurationError: If path does not exist
void rebase_valid_path(std::filesystem::path &path, const std::filesystem::path &base_dir);

/// @brief Get a valid path from a JSON object
/// @param j JSON object
/// @param key Key to value
/// @param base_dir Base directory for relative path
/// @param out Output variable
/// @return True if value was retrieved successfully and is valid path, false otherwise
bool rebase_valid_path_to(const nlohmann::json &j, const std::string &key,
                          std::filesystem::path &out,
                          const std::filesystem::path &base_dir) noexcept;

/// @brief Get a valid path from a JSON object
/// @param j JSON object
/// @param key Key to value
/// @param base_dir Base directory for relative path
/// @param out Output variable
/// @param success Success flag, set to false in case of failure
void rebase_valid_path_to(const nlohmann::json &j, const std::string &key,
                          std::filesystem::path &out, const std::filesystem::path &base_dir,
                          bool &success) noexcept;

/// @brief Load FileInfo from JSON
/// @param node The node of the JSON object containing the FileInfo
/// @param base_dir Base folder
/// @return FileInfo
/// @throw ConfigurationError: Invalid config file format
FileInfo get_file_info(const nlohmann::json &node, const std::filesystem::path &base_dir);

/// @brief Load settings section of JSON
/// @param j Input JSON
/// @return SettingsInfo
/// @throw ConfigurationError: Could not load settings
SettingsInfo get_settings(const nlohmann::json &j);

/// @brief Load BaselineInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @return BaselineInfo
/// @throw ConfigurationError: One or more files could not be found
BaselineInfo get_baseline_info(const nlohmann::json &j, const std::filesystem::path &base_dir);

/// @brief Load interventions from running section
/// @param running Running section of JSON object
/// @param config Config object to update
/// @throw ConfigurationError: Could not load interventions
void load_interventions(const nlohmann::json &running, Configuration &config);

/// @brief Reject deprecated root-level fields when project_requirements is the source of truth.
inline void reject_deprecated_root_config_fields(const nlohmann::json &opt) {
    if (!opt.contains("project_requirements")) {
        return;
    }
    if (opt.contains("trend_type") || opt.contains("income_categories")) {
        throw ConfigurationError{
            "Deprecated root-level trend_type and/or income_categories are not allowed when "
            "project_requirements is present. Use project_requirements.trend and "
            "project_requirements.income.categories instead."};
    }
}

inline void validate_legacy_trend_type_string(const std::string &trend_type) {
    if (trend_type != "null" && trend_type != "trend" && trend_type != "upf_trend" &&
        trend_type != "UPFTrend" && trend_type != "income_trend") {
        throw ConfigurationError{fmt::format("Invalid trend_type: {}. Must be one of: null, trend, "
                                             "upf_trend, UPFTrend, income_trend",
                                             trend_type)};
    }
}

/// @brief Map legacy root-level trend_type / income_categories into project_requirements.
inline void apply_legacy_root_config_fields(const nlohmann::json &opt, ProjectRequirements &req) {
    if (opt.contains("trend_type")) {
        const auto trend_type = opt["trend_type"].get<std::string>();
        validate_legacy_trend_type_string(trend_type);
        req.trend.type = trend_type;
        req.trend.enabled = trend_type != "null";
    }
    if (opt.contains("income_categories")) {
        const auto categories = opt["income_categories"].get<std::string>();
        (void)core::income_category_layout_from_config(categories);
        req.income.categories = categories;
    }
}

/// @brief Parse project_requirements block from config.json.
inline void parse_project_requirements_block(const nlohmann::json &pr, ProjectRequirements &req) {
    const auto &d = pr["demographics"];
    req.demographics.age = d["age"].get<bool>();
    req.demographics.gender = d["gender"].get<bool>();
    req.demographics.region = d["region"].get<bool>();
    req.demographics.ethnicity = d["ethnicity"].get<bool>();
    if (d.contains("max_age_for_linear_models") && !d["max_age_for_linear_models"].is_null()) {
        req.demographics.max_age_for_linear_models = d["max_age_for_linear_models"].get<int>();
    }
    const auto &inc = pr["income"];
    req.income.enabled = inc["enabled"].get<bool>();
    req.income.type = inc["type"].get<std::string>();
    req.income.categories = inc["categories"].get<std::string>();
    req.income.adjust_to_factors_mean = inc["adjust_to_factors_mean"].get<bool>();
    req.income.trended = inc["trended"].get<bool>();
    if (inc.contains("income_based_csv_output")) {
        req.income.income_based_csv_output = inc["income_based_csv_output"].get<bool>();
    }
    const auto &pa = pr["physical_activity"];
    req.physical_activity.enabled = pa["enabled"].get<bool>();
    req.physical_activity.type = pa["type"].get<std::string>();
    req.physical_activity.adjust_to_factors_mean = pa["adjust_to_factors_mean"].get<bool>();
    req.physical_activity.trended = pa["trended"].get<bool>();
    const auto &rf = pr["risk_factors"];
    req.risk_factors.adjust_to_factors_mean = rf["adjust_to_factors_mean"].get<bool>();
    req.risk_factors.trended = rf["trended"].get<bool>();
    const auto &tr = pr["trend"];
    req.trend.enabled = tr["enabled"].get<bool>();
    req.trend.type = tr["type"].get<std::string>();
    const auto &ts = pr["two_stage"];
    req.two_stage.use_logistic = ts["use_logistic"].get<bool>();
    if (ts.contains("logistic_file") && !ts["logistic_file"].is_null()) {
        req.two_stage.logistic_file = ts["logistic_file"].get<std::string>();
    }
}
} // namespace hgps::input
