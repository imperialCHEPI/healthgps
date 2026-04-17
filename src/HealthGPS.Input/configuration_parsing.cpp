#include "configuration_parsing.h"
#include "configuration_parsing_helpers.h"
#include "jsonparser.h"

#include <cstdint>
#include <fmt/color.h>
#include <stdexcept>
#include <unordered_map>

namespace hgps::input {
using json = nlohmann::json;

namespace {

// MAHIMA: Income quintile factor means adjustment (see income_quintile_factor_means_plan.md; Phase
// 2+ simulation behaviour).
/// Optional block under modelling.baseline_adjustments: extra factors-mean CSVs per income stratum.
/// Phase 1 only parses, validates paths when enabled, and stores results on BaselineInfo.
/// Simulation behaviour is unchanged until later phases read these fields.
void parse_income_stratum_factors_mean_block(const json &baseline_adjustments_node,
                                             const std::filesystem::path &base_dir,
                                             IncomeStratumFactorsMeanConfig &out) {
    if (!baseline_adjustments_node.contains("income_stratum_factors_mean")) {
        return;
    }
    const auto &block = baseline_adjustments_node.at("income_stratum_factors_mean");
    if (block.is_null()) {
        return;
    }
    if (!block.is_object()) {
        throw ConfigurationError{
            "baseline_adjustments.income_stratum_factors_mean must be a JSON object"};
    }

    if (block.contains("enabled")) {
        if (!block.at("enabled").is_boolean()) {
            throw ConfigurationError{
                "baseline_adjustments.income_stratum_factors_mean.enabled must be a boolean"};
        }
        out.enabled = block.at("enabled").get<bool>();
    }

    if (block.contains("adjustment_income_stratum_count")) {
        if (!block.at("adjustment_income_stratum_count").is_number_integer()) {
            throw ConfigurationError{"baseline_adjustments.income_stratum_factors_mean."
                                     "adjustment_income_stratum_count must be an integer"};
        }
        // MAHIMA: Parse via int64_t first, reject negatives, then cast to std::size_t. JSON has no
        // unsigned type; direct get<std::size_t>() is awkward for validation, and we want an
        // explicit error for negative values rather than silent wraparound.
        const auto n = block.at("adjustment_income_stratum_count").get<std::int64_t>();
        if (n < 0) {
            throw ConfigurationError{"baseline_adjustments.income_stratum_factors_mean."
                                     "adjustment_income_stratum_count must be non-negative"};
        }
        out.adjustment_income_stratum_count = static_cast<std::size_t>(n);
    }

    if (block.contains("strata")) {
        const auto &strata_json = block.at("strata");
        if (!strata_json.is_array()) {
            throw ConfigurationError{
                "baseline_adjustments.income_stratum_factors_mean.strata must be an array"};
        }
        out.strata.clear();
        out.strata.reserve(strata_json.size());
        for (std::size_t i = 0; i < strata_json.size(); ++i) {
            const auto &row = strata_json.at(i);
            if (!row.is_object()) {
                throw ConfigurationError{fmt::format(
                    "baseline_adjustments.income_stratum_factors_mean.strata[{}] must be an object",
                    i)};
            }
            IncomeStratumFactorsMeanStratumEntry entry;
            entry.id = row.at("id").get<std::string>();
            entry.factorsmean_male = row.at("factorsmean_male").get<std::string>();
            entry.factorsmean_female = row.at("factorsmean_female").get<std::string>();
            out.strata.push_back(std::move(entry));
        }
    }

    if (!out.enabled) {
        // Stratum files are ignored when disabled; do not require paths to exist (easier WIP
        // configs).
        return;
    }

    // --- Validation when enabled (strict) ---
    if (out.adjustment_income_stratum_count < 2) {
        throw ConfigurationError{
            "When income_stratum_factors_mean.enabled is true, "
            "adjustment_income_stratum_count must be >= 2 (rank buckets for adjustment)."};
    }
    // MAHIMA: Compare strata.size() to adjustment_income_stratum_count without casts; both are
    // unsigned (size_t), which fixes clang-tidy modernize-use-integer-sign-comparison from the
    // earlier int vs size_t pattern.
    if (out.strata.size() != out.adjustment_income_stratum_count) {
        throw ConfigurationError{fmt::format(
            "income_stratum_factors_mean: adjustment_income_stratum_count ({}) must equal "
            "the number of strata entries ({})",
            out.adjustment_income_stratum_count, out.strata.size())};
    }

    for (std::size_t i = 0; i < out.strata.size(); ++i) {
        auto &e = out.strata[i];
        if (e.id.empty()) {
            throw ConfigurationError{
                fmt::format("income_stratum_factors_mean.strata[{}]: id must be non-empty", i)};
        }
        try {
            rebase_valid_path(e.factorsmean_male, base_dir);
            rebase_valid_path(e.factorsmean_female, base_dir);
        } catch (const ConfigurationError &ex) {
            throw ConfigurationError{fmt::format(
                "income_stratum_factors_mean.strata[{}] (id=\"{}\"): {}", i, e.id, ex.what())};
        }
        fmt::print("  stratum {:>2} id={:<16} male: {}\n", i, e.id, e.factorsmean_male.string());
        fmt::print("              {:<16} female: {}\n", "", e.factorsmean_female.string());
    }
}

} // namespace

nlohmann::json get(const json &j, const std::string &key) {
    try {
        return j.at(key);
    } catch (const std::exception &) {
        fmt::print(fg(fmt::color::red), "Missing key \"{}\"\n", key);
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
        fmt::print(fg(fmt::color::red), "Could not find file {}\n", out.string());
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
                fmt::print(fg(fmt::color::red), "Could not find file: {}\n", path.string());
                success = false;
            }
        }
    }

    if (!success) {
        throw ConfigurationError{"Could not get baseline adjustments"};
    }
    // MAHIMA: Income quintile factor means adjustment (see income_quintile_factor_means_plan.md;
    // Phase 2+ simulation behaviour).
    //  Optional: per-stratum factors-mean files (income-stratum adjustment feature; see project
    //  plan).
    parse_income_stratum_factors_mean_block(adj, base_dir, info.income_stratum_factors_mean);

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
        config.ses = get(modelling, "ses_model").get<SESInfo>();
    } catch (const std::exception &) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load SES mappings");
    }

    // policy_start_year is optional (schema default 0); missing key must not fail load
    get_to(modelling, "policy_start_year", info.policy_start_year);

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
