#include "configuration_parsing.h"
#include "jsonparser.h"

#include <fmt/color.h>
#include <stdexcept>
#include <unordered_map>

namespace host {
using json = nlohmann::json;

/// @brief Load value from JSON, printing an error message if it fails
/// @param j JSON object
/// @param key Key to value
/// @throw ConfigurationError: Key not found
/// @return Key value
auto get(const json &j, const std::string &key) {
    try {
        return j.at(key);
    } catch (const std::out_of_range &) {
        fmt::print(fg(fmt::color::red), "Missing key \"{}\"\n", key);
        throw ConfigurationError{fmt::format("Missing key \"{}\"", key)};
    }
}

/// @brief Get value from JSON object and store in out
/// @tparam T Type of output object
/// @param j JSON object
/// @param key Key to value
/// @param out Output object
/// @return True if value was retrieved successfully, false otherwise
template <class T> bool get_to(const json &j, const std::string &key, T &out) {
    try {
        j.at(key).get_to(out);
        return true;
    } catch (const std::out_of_range &) {
        fmt::print(fg(fmt::color::red), "Missing key \"{}\"\n", key);
        return false;
    } catch (const json::type_error &) {
        fmt::print(fg(fmt::color::red), "Key \"{}\" is of wrong type\n", key);
        return false;
    }
}

/// @brief Get value from JSON object and store in out, setting success flag
/// @tparam T Type of output object
/// @param j JSON object
/// @param key Key to value
/// @param out Output object
/// @param success Success flag, set to false in case of failure
/// @return True if value was retrieved successfully, false otherwise
template <class T> bool get_to(const json &j, const std::string &key, T &out, bool &success) {
    const bool ret = get_to(j, key, out);
    if (!ret) {
        success = false;
    }
    return ret;
}

/// @brief Rebase path on base_dir
/// @param path Initial path (relative or absolute)
/// @param base_dir New base directory for relative path
/// @throw ConfigurationError: If path does not exist
void rebase_valid_path(std::filesystem::path &path, const std::filesystem::path &base_dir) {
    if (path.is_relative()) {
        path = std::filesystem::weakly_canonical(base_dir / path);
    }

    if (!std::filesystem::exists(path)) {
        throw ConfigurationError{fmt::format("Path does not exist: {}", path.string())};
    }
}

/// @brief Get a valid path from a JSON object
/// @param j JSON object
/// @param key Key to value
/// @param base_dir Base directory for relative path
/// @param out Output variable
/// @return True if value was retrieved successfully and is valid path, false otherwise
bool get_valid_path_to(const json &j, const std::string &key, const std::filesystem::path &base_dir,
                       std::filesystem::path &out) {
    if (!get_to(j, key, out)) {
        return false;
    }

    try {
        rebase_valid_path(out, base_dir);
    } catch (const ConfigurationError &) {
        fmt::print(fg(fmt::color::red), "Could not find file {}", out.string());
        return false;
    }

    return true;
}

/// @brief Get a valid path from a JSON object
/// @param j JSON object
/// @param key Key to value
/// @param base_dir Base directory for relative path
/// @param out Output variable
/// @param success Success flag, set to false in case of failure
void get_valid_path_to(const json &j, const std::string &key, const std::filesystem::path &base_dir,
                       std::filesystem::path &out, bool &success) {
    if (!get_valid_path_to(j, key, base_dir, out)) {
        success = false;
    }
}

/// @brief Load FileInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @return FileInfo
/// @throw ConfigurationError: Invalid config file format
auto get_file_info(const json &j, const std::filesystem::path &base_dir) {
    const auto dataset = get(j, "dataset");

    bool success = true;
    poco::FileInfo info;
    get_valid_path_to(dataset, "name", base_dir, info.name, success);
    get_to(dataset, "format", info.format, success);
    get_to(dataset, "delimiter", info.delimiter, success);
    get_to(dataset, "columns", info.columns, success);
    if (!success) {
        throw ConfigurationError{"Could not load input file info"};
    }

    return info;
}

auto get_settings(const json &j) {
    poco::SettingsInfo info;
    if (!get_to(j, "settings", info)) {
        throw ConfigurationError{"Could not load settings info"};
    }

    return info;
}

/// @brief Load BaselineInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @return BaselineInfo
/// @throw ConfigurationError: One or more files could not be found
auto get_baseline_info(const json &j, const std::filesystem::path &base_dir) {
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

/// @brief Load interventions from running section
/// @param running Running section of JSON object
/// @param config Config object to update
/// @throw ConfigurationError: Could not load interventions
void load_interventions(const json &running, Configuration &config) {
    const auto interventions = get(running, "interventions");

    try {
        // If the type of intervention is null, then there's nothing else to do
        if (interventions.at("active_type_id").is_null()) {
            return;
        }
    } catch (const std::out_of_range &) {
        throw ConfigurationError{"Interventions section missing key \"active_type_id\""};
    }

    const auto active_type_id = [&interventions]() {
        try {
            return interventions["active_type_id"].get<hgps::core::Identifier>();
        } catch (const json::type_error &) {
            throw ConfigurationError{"active_type_id key must be of type string"};
        }
    }();

    /*
     * NB: This loads all of the policy scenario info from the JSON file, which is
     * strictly speaking unnecessary, but it does mean that we can verify the data
     * format is correct.
     */
    std::unordered_map<hgps::core::Identifier, poco::PolicyScenarioInfo> policy_types;
    if (!get_to(interventions, "types", policy_types)) {
        throw ConfigurationError{"Could not load policy types from interventions section"};
    }

    try {
        config.intervention = policy_types.at(active_type_id);
        config.intervention.identifier = active_type_id.to_string();
        config.has_active_intervention = true;
    } catch (const std::out_of_range &) {
        throw ConfigurationError{fmt::format("Unknown active intervention type identifier: {}",
                                             active_type_id.to_string())};
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

void load_input_info(const json &j, Configuration &config,
                     const std::filesystem::path &config_dir) {
    const auto inputs = get(j, "inputs");
    bool success = true;

    // Input dataset file
    try {
        config.file = get_file_info(inputs, config_dir);
        fmt::print("Input dataset file: {}\n", config.file.name.string());
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load dataset file: {}\n", e.what());
    }

    // Settings
    try {
        config.settings = get_settings(inputs);
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load settings info");
    }

    if (!success) {
        throw ConfigurationError{"Could not load settings info"};
    }
}

void load_modelling_info(const json &j, Configuration &config,
                         const std::filesystem::path &config_dir) {
    bool success = true;
    const auto modelling = get(j, "modelling");

    auto &info = config.modelling;
    get_to(modelling, "risk_factors", info.risk_factors, success);

    // Rebase paths and check for errors
    if (get_to(modelling, "risk_factor_models", info.risk_factor_models, success)) {
        for (auto &[type, path] : info.risk_factor_models) {
            try {
                rebase_valid_path(path, config_dir);
                fmt::print("{:<14}, file: {}\n", type, path.string());
            } catch (const ConfigurationError &) {
                success = false;
                fmt::print(fg(fmt::color::red), "Adjustment type: {}, file: {} not found.\n", type,
                           path.string());
            }
        }
    }

    try {
        info.baseline_adjustment = get_baseline_info(modelling, config_dir);
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load baseline adjustment: {}\n", e.what());
    }

    try {
        // SES mapping
        // TODO: Maybe this needs its own helper function
        config.ses = get(modelling, "ses_model").get<poco::SESInfo>();
    } catch (const std::exception &e) {
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
