/**
 * @file
 * @brief Functionality for parsing console application's command-line arguments
 */
#pragma once
#include <cxxopts.hpp>

#include <filesystem>

namespace host {
/// @brief Defines the Command Line Interface (CLI) arguments options
struct CommandOptions {
    /// @brief Indicates whether the argument parsing succeed
    bool success{};

    /// @brief The exit code to return, in case of CLI arguments parsing failure
    int exit_code{};

    /// @brief The configuration file argument value
    std::filesystem::path config_file{};

    /// @brief The back-end storage full path or URL argument value
    std::string data_path_or_url{};

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
/// @return User command-line options
CommandOptions parse_arguments(cxxopts::Options &options, int &argc, char *argv[]);

} // namespace host
