#pragma once

#include <filesystem>

namespace hgps::input {
/// @brief Get cache directory for extracting a file into
/// @param file_hash The SHA256 hash of the file
/// @return The path to the directory
std::filesystem::path get_zip_cache_directory(const std::string &file_hash);

/// @brief Extract a zip file to the specified location
/// @param file_path The path to the zip file
/// @param output_directory The path to the output folder
void extract_zip_file(const std::filesystem::path &file_path,
                      const std::filesystem::path &output_directory);
} // namespace hgps::input
