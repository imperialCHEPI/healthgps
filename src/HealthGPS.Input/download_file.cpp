#include "download_file.h"
#include "HealthGPS/program_dirs.h"

#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <fmt/format.h>

#include <fstream>
#include <random>
#include <sstream>

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
} // anonymous namespace

namespace hgps::input {
void download_file(const std::string &url, const std::filesystem::path &download_path) {
    std::ofstream ofs{download_path, std::ios::binary};
    if (!ofs) {
        throw std::runtime_error(fmt::format("Failed to create file {}", download_path.string()));
    }

    fmt::print("Downloading data from {}\n", url);

    curlpp::Cleanup cleanup;

    // Our request to be sent
    curlpp::Easy request;
    request.setOpt<curlpp::options::Url>(url);
    request.setOpt<curlpp::options::FollowLocation>(true);
    request.setOpt<curlpp::options::WriteStream>(&ofs);

    // Make request
    request.perform();
}

std::filesystem::path download_file_to_temporary(const std::string &url,
                                                 const std::string &file_extension) {
    const auto download_path = get_temporary_file_path("data", file_extension);
    download_file(url, download_path);
    return download_path;
}

} // namespace hgps::input
