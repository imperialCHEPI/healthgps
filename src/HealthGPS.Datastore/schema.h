#pragma once

#include <filesystem>
#include <istream>

//! The prefix for Health-GPS schema URLs
#define HGPS_SCHEMA_URL_PREFIX                                                                     \
    "https://raw.githubusercontent.com/imperialCHEPI/healthgps/main/schemas/v1/"

//! The name of the index.json schema file
#define HGPS_DATA_INDEX_SCHEMA_FILENAME "data_index.json"

//! The schema URL for the data index file
#define HGPS_DATA_INDEX_SCHEMA_URL (HGPS_SCHEMA_URL_PREFIX HGPS_DATA_INDEX_SCHEMA_FILENAME)

namespace hgps::data {
/// @brief Validate the index.json file
/// @param schema_directory The root folder for JSON schemas
/// @param index_stream The input stream for the index.json file
void validate_index(const std::filesystem::path &schema_directory, std::istream &index_stream);
} // namespace hgps::data
