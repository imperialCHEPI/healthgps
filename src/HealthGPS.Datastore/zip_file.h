#pragma once

#include <filesystem>

namespace hgps::data {
//! Create a temporary directory with a unique path
std::filesystem::path create_temporary_directory();

/// @brief Get cache directory for extracting a file into
/// @param file_hash The SHA256 hash of the file
/// @return The path to the directory
std::filesystem::path get_zip_cache_directory(const std::string &file_hash);

/// @brief Extract a zip file to the specified location
/// @param file_path The path to the zip file
/// @param output_directory The path to the output folder
void extract_zip_file(const std::filesystem::path &file_path,
                      const std::filesystem::path &output_directory);

/// @brief Load the contents of the zip file from cache or extract
/// @param file_path Path to zip file
/// @return The path to the cache folder where the contents were extracted
std::filesystem::path extract_zip_file_or_load_from_cache(const std::filesystem::path &file_path);
} // namespace hgps::data
