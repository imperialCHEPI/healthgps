#include "pch.h"

#include "HealthGPS.Input/configuration.h"

#include <fmt/format.h>

#include <filesystem>
#include <vector>

namespace {
namespace fs = std::filesystem;

void get_config_file_paths_inner(const fs::path &dir_path, std::vector<fs::path> &paths_out) {
    for (const auto &entry : fs::directory_iterator{dir_path}) {
        auto path = entry.path();
        if (fs::is_directory(path)) {
            // Recurse
            get_config_file_paths_inner(path, paths_out);
        } else if (fs::is_regular_file(path) && path.filename().string().ends_with("Config.json")) {
            paths_out.push_back(path);
        }
    }
}

// Get the paths to all config files in examples_path
auto get_config_file_paths(const std::filesystem::path &examples_path) {
    std::vector<fs::path> paths;
    get_config_file_paths_inner(examples_path, paths);
    return paths;
}
} // anonymous namespace

// Attempt to load every config file in the specified folder
TEST(ExamplesRegression, TestConfigFilesLoad) {
    const auto config_file_paths = get_config_file_paths(TEST_EXAMPLES_PATH);
    for (const auto &path : config_file_paths) {
        fmt::println("Loading {}", path.string());
        EXPECT_NO_THROW(hgps::input::get_configuration(path, /*job_id=*/0, /*verbose=*/true));
    }
}
