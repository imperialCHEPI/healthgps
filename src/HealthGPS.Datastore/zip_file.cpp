#include "zip_file.h"

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

void extract_zip_file(const std::filesystem::path &file_path,
                      const std::filesystem::path &output_directory) {

    using namespace libzippp;

    ZipArchive zf(file_path);
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
} // namespace hgps::data
