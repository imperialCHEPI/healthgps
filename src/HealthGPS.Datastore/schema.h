#pragma once

#include <filesystem>
#include <istream>

namespace hgps::data {
/// @brief Validate the index.json file
/// @param schema_directory The root folder for JSON schemas
/// @param index_stream The input stream for the index.json file
void validate_index(const std::filesystem::path &schema_directory, std::istream &index_stream);
} // namespace hgps::data
