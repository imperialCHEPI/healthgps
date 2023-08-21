#include "command_options.h"
#include "version.h"

#include <fmt/color.h>

#include <iostream>

namespace host {

cxxopts::Options create_options() {
    cxxopts::Options options("HealthGPS.Console", "Health-GPS microsimulation for policy options.");
    options.add_options()("f,file", "Configuration file full name.", cxxopts::value<std::string>())(
        "s,storage", "Path to root folder of the data storage.", cxxopts::value<std::string>())(
        "j,jobid", "The batch execution job identifier.",
        cxxopts::value<int>())("verbose", "Print more information about progress",
                               cxxopts::value<bool>()->default_value("false"))(
        "help", "Help about this application.")("version", "Print the application version number.");

    return options;
}

CommandOptions parse_arguments(cxxopts::Options &options, int &argc, char *argv[]) {
    namespace fs = std::filesystem;

    CommandOptions cmd;
    try {
        cmd.success = true;
        cmd.exit_code = EXIT_SUCCESS;
        cmd.verbose = false;
        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            cmd.success = false;
            return cmd;
        }

        if (result.count("version")) {
            fmt::print("Version {}\n\n", PROJECT_VERSION);
            cmd.success = false;
            return cmd;
        }

        cmd.verbose = result["verbose"].as<bool>();
        if (cmd.verbose) {
            fmt::print(fg(fmt::color::dark_salmon), "Verbose output enabled\n");
        }

        if (result.count("file")) {
            cmd.config_file = result["file"].as<std::string>();
            if (cmd.config_file.is_relative()) {
                cmd.config_file = std::filesystem::absolute(cmd.config_file);
                fmt::print("Configuration file..: {}\n", cmd.config_file.string());
            }
        }

        if (!fs::exists(cmd.config_file)) {
            fmt::print(fg(fmt::color::red), "\nConfiguration file: {} not found.\n",
                       cmd.config_file.string());
            cmd.exit_code = EXIT_FAILURE;
        }

        if (result.count("storage")) {
            cmd.storage_folder = result["storage"].as<std::string>();
            if (cmd.storage_folder.is_relative()) {
                cmd.storage_folder = std::filesystem::absolute(cmd.storage_folder);
                fmt::print("File storage folder.: {}\n", cmd.storage_folder.string());
            }
        }

        if (!fs::exists(cmd.storage_folder)) {
            fmt::print(fg(fmt::color::red), "\nFile storage folder: {} not found.\n",
                       cmd.storage_folder.string());
            cmd.exit_code = EXIT_FAILURE;
        }

        if (result.count("jobid")) {
            cmd.job_id = result["jobid"].as<int>();
            if (cmd.job_id < 1) {
                fmt::print(fg(fmt::color::red),
                           "\nJob identifier value outside range: (0 < x) given: {}.\n",
                           std::to_string(cmd.job_id));
                cmd.exit_code = EXIT_FAILURE;
            }
        }

        cmd.success = cmd.exit_code == EXIT_SUCCESS;
    } catch (const cxxopts::exceptions::exception &ex) {
        fmt::print(fg(fmt::color::red), "\nInvalid command line argument: {}.\n", ex.what());
        fmt::print("\n{}\n", options.help());
        cmd.success = false;
        cmd.exit_code = EXIT_FAILURE;
    }

    return cmd;
}
} // namespace host