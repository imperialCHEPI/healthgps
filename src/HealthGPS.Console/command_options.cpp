#include "command_options.h"
#include "version.h"

#include <fmt/color.h>

#include <iostream>

namespace hgps {

cxxopts::Options create_options() {
    cxxopts::Options options("HealthGPS.Console", "Health-GPS microsimulation for policy options.");

    // clang-format off
    options.add_options()
        ("f,file", "Path to configuration file, folder or URL (deprecated).",
            cxxopts::value<std::string>())
        ("c,config", "Path to configuration file, folder or URL.", cxxopts::value<std::string>())
        ("s,storage", "Path to root folder of the data storage.", cxxopts::value<std::string>())
        ("j,jobid", "The batch execution job identifier.", cxxopts::value<int>())
        ("o,output", "Path to output folder", cxxopts::value<std::string>())
        ("T,threads", "The maximum number of threads to create (0: no limit, default).",
            cxxopts::value<size_t>())
        ("verbose", "Print more information about progress",
            cxxopts::value<bool>()->default_value("false"))
        ("help", "Help for this application.")
        ("version", "Print the application version number.");
    // clang-format on

    return options;
}

std::optional<CommandOptions> parse_arguments(cxxopts::Options &options, int argc, char **argv) {
    namespace fs = std::filesystem;

    CommandOptions cmd;
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << '\n';
        return std::nullopt;
    }

    if (result.count("version")) {
        fmt::print("Version {}\n\n", PROJECT_VERSION);
        return std::nullopt;
    }

    cmd.verbose = result["verbose"].as<bool>();
    if (cmd.verbose) {
        fmt::print(fg(fmt::color::dark_salmon), "Verbose output enabled\n");
    }

    if (result.count("file")) {
        fmt::print(fg(fmt::color::dark_salmon),
                   "The -f/--file option is deprecated. Use -c/--config instead.\n");
        cmd.config_source = result["file"].as<std::string>();
    }
    if (result.count("config")) {
        cmd.config_source = result["config"].as<std::string>();
    }
    fmt::print("Configuration source: {}\n", cmd.config_source);

    if (result.count("storage")) {
        auto source = result["storage"].as<std::string>();

        fmt::print(fmt::fg(fmt::color::yellow),
                   "WARNING: Path to data source specified with command-line argument. "
                   "This functionality is deprecated and will be removed in future. You "
                   "should pass the data source via the config file.\n");
        fmt::print("Data source: {}\n", source);

        cmd.data_source = hgps::input::DataSource(std::move(source));
    }

    if (result.count("output")) {
        cmd.output_folder = result["output"].as<std::string>();
    }

    if (result.count("jobid")) {
        cmd.job_id = result["jobid"].as<int>();
        if (cmd.job_id < 1) {
            throw std::runtime_error(
                fmt::format("Job identifier value outside range: (0 < x) given: {}.", cmd.job_id));
        }
    }

    if (result.count("threads")) {
        cmd.num_threads = result["threads"].as<size_t>();
    }

    return cmd;
}
} // namespace hgps
