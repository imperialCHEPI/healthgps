#include "configuration_parsing.h"
#include "configuration_parsing_helpers.h"
#include "jsonparser.h"

#include <fmt/color.h>
#include <stdexcept>
#include <unordered_map>

namespace hgps::input {
using json = nlohmann::json;

nlohmann::json get(const json &j, const std::string &key) {
    try {
        return j.at(key);
    } catch (const std::exception &) {
        fmt::print(fmt::fg(fmt::color::red), "Missing key \"{}\"\n", key);
        throw ConfigurationError{fmt::format("Missing key \"{}\"", key)};
    }
}

void rebase_valid_path(std::filesystem::path &path, const std::filesystem::path &base_dir) try {
    if (path.is_relative()) {
        path = std::filesystem::absolute(base_dir / path);
    }

    if (!std::filesystem::exists(path)) {
        throw ConfigurationError{fmt::format("Path does not exist: {}", path.string())};
    }
} catch (const std::filesystem::filesystem_error &) {
    throw ConfigurationError{fmt::format("OS error while reading path {}", path.string())};
}

// NOLINTNEXTLINE(bugprone-exception-escape)
bool rebase_valid_path_to(const json &j, const std::string &key, std::filesystem::path &out,
                          const std::filesystem::path &base_dir) noexcept {
    if (!get_to(j, key, out)) {
        return false;
    }

    try {
        rebase_valid_path(out, base_dir);
    } catch (const ConfigurationError &) {
        fmt::print(fmt::fg(fmt::color::red), "Could not find file {}\n", out.string());
        return false;
    }

    return true;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
void rebase_valid_path_to(const json &j, const std::string &key, std::filesystem::path &out,
                          const std::filesystem::path &base_dir, bool &success) noexcept {
    if (!rebase_valid_path_to(j, key, out, base_dir)) {
        success = false;
    }
}

FileInfo get_file_info(const json &node, const std::filesystem::path &base_dir) {
    bool success = true;
    FileInfo info;
    rebase_valid_path_to(node, "name", info.name, base_dir, success);
    get_to(node, "format", info.format, success);
    get_to(node, "delimiter", info.delimiter, success);
    get_to(node, "columns", info.columns, success);
    if (!success) {
        throw ConfigurationError{"Could not load input file info"};
    }

    return info;
}

SettingsInfo get_settings(const json &j) {
    SettingsInfo info;
    if (!get_to(j, "settings", info)) {
        throw ConfigurationError{"Could not load settings info"};
    }

    return info;
}

BaselineInfo get_baseline_info(const json &j, const std::filesystem::path &base_dir) {
    const auto &adj = get(j, "baseline_adjustments");

    bool success = true;
    BaselineInfo info;
    get_to(adj, "format", info.format, success);
    get_to(adj, "delimiter", info.delimiter, success);
    get_to(adj, "encoding", info.encoding, success);
    if (get_to(adj, "file_names", info.file_names, success)) {
        // Rebase paths and check for errors
        for (auto &[name, path] : info.file_names) {
            try {
                rebase_valid_path(path, base_dir);
                fmt::print("{:<14}, file: {}\n", name, path.string());
            } catch (const ConfigurationError &) {
                fmt::print(fmt::fg(fmt::color::red), "Could not find file: {}\n", path.string());
                success = false;
            }
        }
    }

    if (!success) {
        throw ConfigurationError{"Could not get baseline adjustments"};
    }

    return info;
}

void load_interventions(const json &running, Configuration &config) {
    const auto interventions = get(running, "interventions");

    bool success = true;

    std::optional<hgps::core::Identifier> active_type_id; // might be null
    if (!get_to(interventions, "active_type_id", active_type_id, success)) {
        fmt::print(fmt::fg(fmt::color::red), "active_type_id is invalid\n");
    }

    /*
     * NB: This loads all of the policy scenario info from the JSON file, which is
     * strictly speaking unnecessary, but it does mean that we can verify the data
     * format is correct.
     */
    std::unordered_map<hgps::core::Identifier, PolicyScenarioInfo> policy_types;
    if (!get_to(interventions, "types", policy_types, success)) {
        fmt::print(fmt::fg(fmt::color::red),
                   "Could not load policy types from interventions section\n");
    }

    if (active_type_id) {
        try {
            config.active_intervention = policy_types.at(active_type_id.value());
            config.active_intervention->identifier = active_type_id->to_string();
        } catch (const std::out_of_range &) {
            success = false;
            fmt::print(fmt::fg(fmt::color::red),
                       "Unknown active intervention type identifier: {}\n",
                       active_type_id->to_string());
        }
    }

    if (!success) {
        throw ConfigurationError{"Failed to load policy interventions"};
    }
}

void check_version(const json &j) {
    int version;
    if (!get_to(j, "version", version)) {
        throw ConfigurationError{"File must have a schema version"};
    }

    if (version != 2) {
        throw ConfigurationError{
            fmt::format("Configuration schema version: {} mismatch, supported: 2", version)};
    }
}

void load_input_info(const json &j, Configuration &config) {
    const auto inputs = get(j, "inputs");
    bool success = true;

    // Input dataset file
    try {
        config.file = get_file_info(get(inputs, "dataset"), config.root_path);
        fmt::print("Input dataset file: {}\n", config.file.name.string());
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load dataset file: {}\n", e.what());
    }

    // Settings
    try {
        config.settings = get_settings(inputs);
    } catch (const std::exception &) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load settings info\n");
    }

    if (!success) {
        throw ConfigurationError{"Could not load settings info"};
    }
}

void load_modelling_info(const json &j, Configuration &config) {
    bool success = true;
    const auto modelling = get(j, "modelling");

    auto &info = config.modelling;
    get_to(modelling, "risk_factors", info.risk_factors, success);

    // Rebase paths and check for errors
    if (get_to(modelling, "risk_factor_models", info.risk_factor_models, success)) {
        for (auto &[type, path] : info.risk_factor_models) {
            try {
                rebase_valid_path(path, config.root_path);
                fmt::print("{:<14}, file: {}\n", type, path.string());
            } catch (const ConfigurationError &) {
                success = false;
                fmt::print(fmt::fg(fmt::color::red), "Adjustment type: {}, file: {} not found.\n",
                           type, path.string());
            }
        }
    }

    try {
        info.baseline_adjustment = get_baseline_info(modelling, config.root_path);
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load baseline adjustment: {}\n", e.what());
    }

    try {
        // SES mapping
        // TODO: Maybe this needs its own helper function
        config.ses = get(modelling, "ses_model").get<SESInfo>();
    } catch (const std::exception &) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load SES mappings\n");
    }

    if (!success) {
        throw ConfigurationError("Could not load modelling info");
    }
}

void load_running_info(const json &j, Configuration &config) {
    const auto running = get(j, "running");

    bool success = true;
    get_to(running, "start_time", config.start_time, success);
    get_to(running, "stop_time", config.stop_time, success);
    get_to(running, "trial_runs", config.trial_runs, success);
    get_to(running, "sync_timeout_ms", config.sync_timeout_ms, success);
    get_to(running, "diseases", config.diseases, success);

    {
        // I copied this logic from the old code, but it seems strange to me. Why do we
        // store multiple seeds but only use the first? -- Alex
        std::vector<unsigned int> seeds;
        if (get_to(running, "seed", seeds, success) && !seeds.empty()) {
            config.custom_seed = seeds[0];
        }
    }

    // Intervention Policy
    try {
        load_interventions(running, config);
    } catch (const ConfigurationError &e) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load interventions: {}", e.what());
    }

    if (!success) {
        throw ConfigurationError{"Could not load running info"};
    }
}

void load_output_info(const json &j, Configuration &config,
                      const std::optional<std::string> &output_folder) {
    if (!get_to(j, "output", config.output)) {
        throw ConfigurationError{"Could not load output info"};
    }

    if (output_folder.has_value() != config.output.folder.empty()) {
        throw ConfigurationError(
            "Must specify output folder via command line argument or config file, but not both");
    }

    if (output_folder.has_value()) {
        config.output.folder = output_folder.value();
    } else {
        config.output.folder = expand_environment_variables(config.output.folder);
        fmt::print(fmt::fg(fmt::color::dark_salmon),
                   "Providing output folder via config file is deprecated. Please use -o "
                   "command-line argument instead.\n");
    }
}

} // namespace hgps::input
