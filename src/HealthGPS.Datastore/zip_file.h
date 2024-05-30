#pragma once

#include <filesystem>

namespace hgps::data {
//! Create a temporary directory with a unique path
std::filesystem::path create_temporary_directory();

/// @brief Extract a zip file to the specified location
/// @param file_path The path to the zip file
/// @param output_directory The path to the output folder
void extract_zip_file(const std::filesystem::path &file_path,
                      const std::filesystem::path &output_directory);
} // namespace hgps::data
