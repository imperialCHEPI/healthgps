#include "download_file.h"
#include "HealthGPS/program_dirs.h"

#include <fmt/format.h>
#include <fstream>
#include <random>
#include <sstream>
#include <cstdlib>
#include <stdexcept>

namespace {
std::filesystem::path get_temporary_file_path(const std::string &file_prefix,
                                              const std::string &file_extension) {
    const auto tmp_dir = hgps::get_temporary_directory();
    std::filesystem::create_directories(tmp_dir);

    std::mt19937 prng(std::random_device{}());
    std::uniform_int_distribution<unsigned> rand;
    std::filesystem::path path;
    while (true) {
        std::stringstream ss;
        ss << file_prefix << std::hex << rand(prng) << file_extension;
        path = tmp_dir / ss.str();
        if (!std::filesystem::exists(path)) {
            return path;
        }
    }
}

// Use system curl or wget to download file
bool download_with_system_tool(const std::string &url, const std::filesystem::path &output_path) {
    // Try curl first
    auto curl_cmd = fmt::format("curl -L -o \"{}\" \"{}\"", output_path.string(), url);
    int curl_result = system(curl_cmd.c_str());
    
    if (curl_result == 0 && std::filesystem::exists(output_path)) {
        return true;
    }
    
    // Try wget as fallback
    auto wget_cmd = fmt::format("wget -O \"{}\" \"{}\"", output_path.string(), url);
    int wget_result = system(wget_cmd.c_str());
    
    return (wget_result == 0 && std::filesystem::exists(output_path));
}
} // anonymous namespace

namespace hgps::input {
void download_file(const std::string &url, const std::filesystem::path &download_path) {
    fmt::print("Downloading data from {}\n", url);

    if (!download_with_system_tool(url, download_path)) {
        throw std::runtime_error(fmt::format("Failed to download file from {} to {}", url, download_path.string()));
    }
    
    if (!std::filesystem::exists(download_path)) {
        throw std::runtime_error(fmt::format("Download succeeded but file {} was not created", download_path.string()));
    }
}

std::filesystem::path download_file_to_temporary(const std::string &url,
                                                 const std::string &file_extension) {
    const auto download_path = get_temporary_file_path("data", file_extension);
    download_file(url, download_path);
    return download_path;
}

} // namespace hgps::input
