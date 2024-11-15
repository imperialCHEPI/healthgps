/**
 * @file
 * @brief This file contains functions for loading subsections of the main JSON file
 */
#pragma once
#include "configuration.h"

#include <optional>

namespace hgps::input {
/// @brief Check the schema version and throw if invalid
/// @param j The root JSON object
/// @throw ConfigurationError: If version attribute is not present or invalid
void check_version(const nlohmann::json &j);

/// @brief Load input dataset
/// @param j The root JSON object
/// @param config The config object to update
/// @throw ConfigurationError: Could not load input dataset
void load_input_info(const nlohmann::json &j, Configuration &config);

/// @brief Load ModellingInfo from JSON
/// @param j The root JSON object
/// @param config The config object to update
/// @throw ConfigurationError: Could not load modelling info
void load_modelling_info(const nlohmann::json &j, Configuration &config);

/// @brief Load running section of JSON object
/// @param j The root JSON object
/// @param config The config object to update
/// @throw ConfigurationError: Could not load running section
void load_running_info(const nlohmann::json &j, Configuration &config);

/// @brief Load output section of JSON object
/// @param j The root JSON object
/// @param config The config object to update
/// @param output_folder Output folder, if provided via command-line argument
/// @throw ConfigurationError: Could not load output info
void load_output_info(const nlohmann::json &j, Configuration &config,
                      const std::optional<std::string> &output_folder);
} // namespace hgps::input
