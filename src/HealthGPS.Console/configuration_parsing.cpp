#include "configuration_parsing.h"
#include "configuration_parsing_helpers.h"
#include "jsonparser.h"

#include <fmt/color.h>
#include <stdexcept>
#include <unordered_map>

namespace host {
using json = nlohmann::json;

nlohmann::json get(const json &j, const std::string &key) {
    try {
        return j.at(key);
    } catch (const std::exception &) {
        fmt::print(fg(fmt::color::red), "Missing key \"{}\"\n", key);
        throw ConfigurationError{fmt::format("Missing key \"{}\"", key)};
    }
}

void rebase_valid_path(std::filesystem::path &path, const std::filesystem::path &base_dir) {
    if (path.is_relative()) {
        try {
            path = std::filesystem::weakly_canonical(base_dir / path);
        } catch (const std::filesystem::filesystem_error &e) {
            throw ConfigurationError{fmt::format("OS error while reading {}", path.string())};
        }
    }

    if (!std::filesystem::exists(path)) {
        throw ConfigurationError{fmt::format("Path does not exist: {}", path.string())};
    }
}

bool rebase_valid_path_to(const json &j, const std::string &key, std::filesystem::path &path,
                          const std::filesystem::path &base_dir) noexcept {
    if (!get_to(j, key, path)) {
        return false;
    }

    try {
        rebase_valid_path(path, base_dir);
    } catch (const ConfigurationError &) {
        fmt::print(fg(fmt::color::red), "Could not find file {}\n", path.string());
        return false;
    }

    return true;
}

void rebase_valid_path_to(const json &j, const std::string &key, std::filesystem::path &path,
                          const std::filesystem::path &base_dir, bool &success) noexcept {
    if (!rebase_valid_path_to(j, key, path, base_dir)) {
        success = false;
    }
}

poco::FileInfo get_file_info(const json &node, const std::filesystem::path &base_dir) {
    bool success = true;
    poco::FileInfo info;
    rebase_valid_path_to(node, "name", info.name, base_dir, success);
    get_to(node, "format", info.format, success);
    get_to(node, "delimiter", info.delimiter, success);
    get_to(node, "columns", info.columns, success);
    if (!success) {
        throw ConfigurationError{"Could not load input file info"};
    }

    return info;
}

poco::SettingsInfo get_settings(const json &j) {
    poco::SettingsInfo info;
    if (!get_to(j, "settings", info)) {
        throw ConfigurationError{"Could not load settings info"};
    }

    return info;
}

poco::BaselineInfo get_baseline_info(const json &j, const std::filesystem::path &base_dir) {
    const auto &adj = get(j, "baseline_adjustments");

    bool success = true;
    poco::BaselineInfo info;
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
                fmt::print(fg(fmt::color::red), "Could not find file: {}\n", path.string());
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
    if (!get_to(interventions, "active_type_id", active_type_id)) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "active_type_id is invalid\n");
    }

    /*
     * NB: This loads all of the policy scenario info from the JSON file, which is
     * strictly speaking unnecessary, but it does mean that we can verify the data
     * format is correct.
     */
    std::unordered_map<hgps::core::Identifier, poco::PolicyScenarioInfo> policy_types;
    if (!get_to(interventions, "types", policy_types)) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red),
                   "Could not load policy types from interventions section\n");
    }

    if (active_type_id) {
        try {
            config.intervention = policy_types.at(active_type_id.value());
            config.intervention.identifier = active_type_id->to_string();
            config.has_active_intervention = true;
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
        fmt::print(fg(fmt::color::red), "Could not load dataset file: {}\n", e.what());
    }

    // Settings
    try {
        config.settings = get_settings(inputs);
    } catch (const std::exception &) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load settings info");
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
                fmt::print(fg(fmt::color::red), "Adjustment type: {}, file: {} not found.\n", type,
                           path.string());
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
        config.ses = get(modelling, "ses_model").get<poco::SESInfo>();
    } catch (const std::exception &) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load SES mappings");
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

void load_output_info(const json &j, Configuration &config) {
    if (!get_to(j, "output", config.output)) {
        throw ConfigurationError{"Could not load output info"};
    }

    config.output.folder = expand_environment_variables(config.output.folder);
}

} // namespace host
