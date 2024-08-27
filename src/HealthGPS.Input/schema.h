#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>

namespace hgps::input {
/// @brief Load a JSON file and validate against the specified schema
/// @param file_path The path to the JSON file
/// @param schema_file_name The name of the JSON schema file
/// @param schema_version The version of the schema file
/// @param require_schema_property Whether to raise an exception if the $schema property
///                                is missing
nlohmann::json load_and_validate_json(const std::filesystem::path &file_path,
                                      const char *schema_file_name, int schema_version,
                                      bool require_schema_property = true);
} // namespace hgps::input
