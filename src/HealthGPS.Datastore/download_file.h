#pragma once

#include <filesystem>
#include <string>

namespace hgps::data {
std::filesystem::path download_file(const std::string &url,
                                    const std::filesystem::path &output_directory);
}
