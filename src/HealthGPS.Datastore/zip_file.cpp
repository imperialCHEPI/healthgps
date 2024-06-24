#include "zip_file.h"
#include "HealthGPS/program_dirs.h"
#include "HealthGPS/sha256.h"

#include <fmt/format.h>
#include <libzippp.h>

#include <fstream>

namespace {
/// @brief Create a temporary directory for extracting a zip file into
/// @param output_path The final output path where files will be moved to
/// @return The path to the temporary directory
std::filesystem::path create_temporary_extract_directory(const std::filesystem::path &output_path) {
    // TODO: Randomise temp path
    std::filesystem::path temp_path = output_path.string() + ".tmp";

    // Perhaps extraction was stopped halfway through? Delete it and start over
    if (std::filesystem::exists(temp_path)) {
        std::filesystem::remove_all(temp_path);
    }

    if (!std::filesystem::create_directories(temp_path)) {
        throw std::runtime_error(
            fmt::format("Failed to create temporary directory: {}", temp_path.string()));
    }

    return temp_path;
}
} // anonymous namespace

namespace hgps::data {
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

    // Extract to temporary folder first, so that the final extraction path should only exist if the
    // operation completed successfully
    const auto temp_output_directory = create_temporary_extract_directory(output_directory);
    std::filesystem::path out_path;
    for (const auto &entry : zf.getEntries()) {
        out_path = temp_output_directory / entry.getName();
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

    // This operation should be atomic
    std::filesystem::rename(temp_output_directory, output_directory);
}

std::filesystem::path extract_zip_file_or_load_from_cache(const std::filesystem::path &file_path) {
    const auto file_hash = compute_sha256_for_file(file_path);
    auto cache_path = get_zip_cache_directory(file_hash);
    if (std::filesystem::exists(cache_path)) {
        return cache_path;
    }

    extract_zip_file(file_path, cache_path);

    return cache_path;
}
} // namespace hgps::data
