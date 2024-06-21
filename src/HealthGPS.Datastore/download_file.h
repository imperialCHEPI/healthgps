#pragma once

#include <filesystem>
#include <string>

namespace hgps::data {
/// @brief Download the file at the specified URL
/// @param url URL to download from
/// @param download_path Destination for downloaded files, including filename
/// @return Path to downloaded file
std::filesystem::path download_file(const std::string &url,
                                    const std::filesystem::path &download_path);

/// @brief Download the file at the specified URL to a temporary folder
/// @param url URL to download from
/// @param file_extension File extension (including dot) for downloaded file
/// @return Path to downloaded file
std::filesystem::path download_file_to_temporary(const std::string &url,
                                                 const std::string &file_extension);
} // namespace hgps::data
