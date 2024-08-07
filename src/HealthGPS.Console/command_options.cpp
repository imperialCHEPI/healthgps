#include "command_options.h"
#include "version.h"

#include <fmt/color.h>

#include <iostream>

namespace hgps {

cxxopts::Options create_options() {
    cxxopts::Options options("HealthGPS.Console", "Health-GPS microsimulation for policy options.");
    options.add_options()("c,config", "Configuration path.", cxxopts::value<std::string>())(
        "f,file", "Configuration path.", cxxopts::value<std::string>())(
        "s,storage", "Path to root folder of the data storage.", cxxopts::value<std::string>())(
        "j,jobid", "The batch execution job identifier.",
        cxxopts::value<int>())("verbose", "Print more information about progress",
                               cxxopts::value<bool>()->default_value("false"))(
        "help", "Help about this application.")("version", "Print the application version number.");

    return options;
}

std::optional<CommandOptions> parse_arguments(cxxopts::Options &options, int argc, char **argv) {
    namespace fs = std::filesystem;

    CommandOptions cmd;
    cmd.verbose = false;
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

    if (result.count("config") && result.count("file")) {
        throw std::runtime_error("Cannot provide both -c/--config and -f/--file arguments.");
    }

    std::filesystem::path config_path;
    if (result.count("file")) {
        fmt::print(
            fmt::fg(fmt::color::yellow),
            "WARNING: The -f/--file options are deprecated. Please use -c/--config instead.\n");

        config_path = result["file"].as<std::string>();
    } else {
        config_path = result["config"].as<std::string>();
    }
    if (config_path.is_relative()) {
        config_path = std::filesystem::absolute(config_path);
    }

    if (config_path.extension() == ".json") {
        fmt::print(fmt::fg(fmt::color::yellow),
                   "WARNING: Providing a path to a config file is deprecated. "
                   "Please provide the path to the parent config directory instead.\n");
        if (config_path.filename() != "config.json") {
            fmt::print(fmt::fg(fmt::color::yellow),
                       "WARNING: Config files should be named config.json.\n");
        }

        cmd.config_file = std::move(config_path);
    } else {
        cmd.config_file = config_path / "config.json";
    }

    if (!fs::exists(cmd.config_file)) {
        throw std::runtime_error(
            fmt::format("Configuration file: {} not found.", cmd.config_file.string()));
    }
    fmt::print("Configuration file: {}\n", cmd.config_file.string());

    if (result.count("storage")) {
        auto source = result["storage"].as<std::string>();

        fmt::print(fmt::fg(fmt::color::yellow),
                   "WARNING: Path to data source specified with command-line argument. "
                   "This functionality is deprecatated and will be removed in future. You "
                   "should pass the data source via the config file.\n");
        fmt::print("Data source: {}\n", source);

        cmd.data_source = hgps::input::DataSource(std::move(source));
    }

    if (result.count("jobid")) {
        cmd.job_id = result["jobid"].as<int>();
        if (cmd.job_id < 1) {
            throw std::runtime_error(
                fmt::format("Job identifier value outside range: (0 < x) given: {}.", cmd.job_id));
        }
    }

    return cmd;
}
} // namespace hgps
