#pragma once
#include "configuration.h"
#include "poco.h"

#include <fmt/color.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

namespace host {
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
poco::FileInfo get_file_info(const nlohmann::json &node, const std::filesystem::path &base_dir);

/// @brief Load settings section of JSON
/// @param j Input JSON
/// @return SettingsInfo
/// @throw ConfigurationError: Could not load settings
poco::SettingsInfo get_settings(const nlohmann::json &j);

/// @brief Load BaselineInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @return BaselineInfo
/// @throw ConfigurationError: One or more files could not be found
poco::BaselineInfo get_baseline_info(const nlohmann::json &j,
                                     const std::filesystem::path &base_dir);

/// @brief Load interventions from running section
/// @param running Running section of JSON object
/// @param config Config object to update
/// @throw ConfigurationError: Could not load interventions
void load_interventions(const nlohmann::json &running, Configuration &config);
} // namespace host
