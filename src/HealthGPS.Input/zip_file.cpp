#include "zip_file.h"
#include "HealthGPS/program_dirs.h"
#include "HealthGPS/sha256.h"

#include <fmt/format.h>
#include <libzippp.h>

#include <fstream>
#include <random>
#include <sstream>

namespace {
/// @brief Create a temporary directory for extracting a zip file into
/// @param output_path The final output path where files will be moved to
/// @return The path to the temporary directory
std::filesystem::path create_temporary_extract_directory(const std::filesystem::path &output_path) {
    // Randomise path in case other processes are also performing extraction
    std::filesystem::path temp_path;
    std::mt19937 rand(std::random_device{}());
    while (true) {
        std::stringstream ss;
        ss << output_path.string() << '.' << std::hex << rand() << ".tmp";
        temp_path = ss.str();

        if (!std::filesystem::create_directories(temp_path)) {
            // Path already exists; try again
            if (std::filesystem::exists(temp_path)) {
                continue;
            }

            // Something else went wrong
            throw std::runtime_error(
                fmt::format("Failed to create temporary directory: {}", temp_path.string()));
        }

        return temp_path;
    }
}
} // anonymous namespace

namespace hgps::input {
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
            // Names seem to have a trailing slash which confuses Windows, hence:
            out_path /= ".";
            if (!std::filesystem::create_directories(out_path)) {
                throw std::runtime_error{
                    fmt::format("Failed to create directory: {}", out_path.string())};
            }
        } else {
            // NB: We assume all files are text files
            std::ofstream ofs{out_path};
            if (!ofs) {
                throw std::runtime_error{
                    fmt::format("Failed to create file: {}", out_path.string())};
            }

            ofs << entry.readAsText();
        }
    }

    try {
        // This operation should be atomic
        std::filesystem::rename(temp_output_directory, output_directory);
    } catch (const std::filesystem::filesystem_error &) {
        // It is possible (albeit unlikely) that another process has successfully completed the
        // extraction while we were attempting to do the same. In this case, we can just delete our
        // temporary folder and carry on.
        if (std::filesystem::exists(output_directory)) {
            std::filesystem::remove_all(temp_output_directory);
        } else {
            // Something else went wrong
            throw;
        }
    }
}
} // namespace hgps::input
