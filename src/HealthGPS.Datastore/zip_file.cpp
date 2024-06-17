#include "zip_file.h"
#include "HealthGPS/program_dirs.h"
#include "HealthGPS/sha256.h"

#include <fmt/format.h>
#include <libzippp.h>

#include <fstream>
#include <random>

namespace hgps::data {
std::filesystem::path create_temporary_directory() {
    auto tmp_dir = std::filesystem::temp_directory_path();
    std::random_device dev;
    std::mt19937 prng(dev());
    std::uniform_int_distribution<unsigned> rand;
    std::filesystem::path path;

    while (true) {
        std::stringstream ss;
        ss << std::hex << rand(prng);
        path = tmp_dir / ss.str();
        // true if the directory was created.
        if (std::filesystem::create_directory(path)) {
            return path;
        }
    }
}

std::filesystem::path get_zip_cache_directory(const std::string &file_hash) {
    if (file_hash.size() != 64) {
        throw std::invalid_argument("file_hash does not appear to be a valid SHA256 hash");
    }

    return get_cache_directory() / "zip" / file_hash.substr(0, 2) / file_hash.substr(2);
}

void extract_zip_file(const std::filesystem::path &file_path,
                      const std::filesystem::path &output_directory) {

    using namespace libzippp;

    ZipArchive zf(file_path.string());
    zf.open();

    std::filesystem::path out_path;
    for (const auto &entry : zf.getEntries()) {
        out_path = output_directory / entry.getName();
        if (entry.isDirectory()) {
            if (!std::filesystem::create_directories(out_path)) {
                throw std::runtime_error{
                    fmt::format("Failed to create directory: {}", out_path.string())};
            }
        } else {
            std::ofstream ofs{out_path};
            if (!ofs) {
                throw std::runtime_error{
                    fmt::format("Failed to create file: {}", out_path.string())};
            }

            ofs << entry.readAsText();
        }
    }
}

std::filesystem::path extract_zip_file_or_load_from_cache(const std::filesystem::path &file_path) {
    const auto file_hash = compute_sha256_for_file(file_path);
    auto cache_path = get_zip_cache_directory(file_hash);
    if (std::filesystem::exists(cache_path)) {
        return cache_path;
    }

    if (!std::filesystem::create_directories(cache_path)) {
        throw std::runtime_error(
            fmt::format("Failed to create cache directory: {}", cache_path.string()));
    }

    extract_zip_file(file_path, cache_path);

    return cache_path;
}
} // namespace hgps::data
