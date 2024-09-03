/**
 * @file
 * @brief Functionality for parsing console application's command-line arguments
 */
#pragma once

#include <cxxopts.hpp>

#include "HealthGPS.Input/data_source.h"

#include <filesystem>
#include <optional>

namespace hgps {
/// @brief Defines the Command Line Interface (CLI) arguments options
struct CommandOptions {
    /// @brief The configuration file argument value
    std::filesystem::path config_file;

    /// @brief The back-end storage full path or URL argument value
    std::optional<hgps::input::DataSource> data_source;

    /// @brief The output folder where results will be saved
    std::optional<std::string> output_folder;

    /// @brief Indicates whether the application logging is verbose
    bool verbose{};

    /// @brief The batch job identifier value, optional.
    int job_id{};
};

/// @brief Creates the command-line interface (CLI) options
/// @return Health-GPS CLI options
cxxopts::Options create_options();

/// @brief Parses the command-line interface (CLI) arguments
/// @param options The valid CLI options
/// @param argc Number of input arguments
/// @param argv List of input arguments
/// @return User command-line options or std::nullopt if program should exit
std::optional<CommandOptions> parse_arguments(cxxopts::Options &options, int argc, char **argv);

} // namespace hgps
