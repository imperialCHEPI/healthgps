#include "configuration.h"
#include "jsonparser.h"
#include "version.h"

#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/fiscal_scenario.h"
#include "HealthGPS/food_labelling_scenario.h"
#include "HealthGPS/marketing_dynamic_scenario.h"
#include "HealthGPS/marketing_scenario.h"
#include "HealthGPS/mtrandom.h"
#include "HealthGPS/physical_activity_scenario.h"
#include "HealthGPS/simple_policy_scenario.h"

#include "HealthGPS.Core/poco.h"
#include "HealthGPS.Core/scoped_timer.h"

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <thread>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                                         \
    hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

namespace host {
using namespace hgps;
using json = nlohmann::json;

ConfigurationError::ConfigurationError(const std::string &msg) : std::runtime_error{msg} {}

auto get_key(const json &j, const std::string &key) {
    try {
        return j.at(key);
    } catch (const std::out_of_range &) {
        fmt::print(fg(fmt::color::red), "Missing key \"{}\"\n", key);
        throw ConfigurationError{fmt::format("Missing key \"{}\"", key)};
    }
}

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

template <class T> bool get_to(const json &j, const std::string &key, T &out, bool &success) {
    const bool ret = get_to(j, key, out);
    if (!ret) {
        success = false;
    }
    return ret;
}

void rebase_path(std::filesystem::path &path, const std::filesystem::path &base_dir) {
    if (path.is_relative()) {
        path = std::filesystem::weakly_canonical(base_dir / path);
    }

    if (!std::filesystem::exists(path)) {
        throw ConfigurationError{fmt::format("Path does not exist: {}", path.string())};
    }
}

/// @brief Get a path, based on base_dir, and check if it exists
/// @param j Input JSON
/// @param base_dir Base folder
/// @return An absolute path, assuming that base_dir is the base if relative
/// @throw json::type_error: Invalid JSON types
/// @throw ConfigurationError: Path does not exist
std::filesystem::path get_valid_path(const json &j, const std::filesystem::path &base_dir) {
    auto path = j.get<std::filesystem::path>();
    rebase_path(path, base_dir);
    return path;
}

bool get_valid_path_to(const json &j, const std::string &key, const std::filesystem::path &base_dir,
                       std::filesystem::path &out) {
    if (!get_to(j, key, out)) {
        return false;
    }

    try {
        rebase_path(out, base_dir);
    } catch (const ConfigurationError &) {
        fmt::print(fg(fmt::color::red), "Could not find file {}", out.string());
        return false;
    }

    return true;
}

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
    const auto dataset = get_key(j, "dataset");

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

/// @brief Load BaselineInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @return BaselineInfo
/// @throw json::type_error: Invalid JSON types
/// @throw ConfigurationError: One or more files could not be found
auto get_baseline_info(const json &j, const std::filesystem::path &base_dir) {
    const auto &adj = get_key(j, "baseline_adjustments");

    bool success = true;
    poco::BaselineInfo info;
    get_to(adj, "format", info.format, success);
    get_to(adj, "delimiter", info.delimiter, success);
    get_to(adj, "encoding", info.encoding, success);
    if (get_to(adj, "file_names", info.file_names, success)) {
        // Rebase paths and check for errors
        for (auto &[name, path] : info.file_names) {
            try {
                rebase_path(path, base_dir);
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

/// @brief Load ModellingInfo from JSON
/// @param j Input JSON
/// @param base_dir Base folder
/// @throw json::type_error: Invalid JSON types
/// @throw ConfigurationError: Could not load modelling info
void load_modelling_info(const json &j, const std::filesystem::path &base_dir,
                         Configuration &config) {
    bool success = true;
    const auto modelling = get_key(j, "modelling");

    auto &info = config.modelling;
    get_to(modelling, "risk_factors", info.risk_factors, success);

    // Rebase paths and check for errors
    if (get_to(modelling, "risk_factor_models", info.risk_factor_models, success)) {
        for (auto &[type, path] : info.risk_factor_models) {
            try {
                rebase_path(path, base_dir);
                fmt::print("{:<14}, file: {}\n", type, path.string());
            } catch (const ConfigurationError &) {
                success = false;
                fmt::print(fg(fmt::color::red), "Adjustment type: {}, file: {} not found.\n", type,
                           path.string());
            }
        }
    }

    try {
        info.baseline_adjustment = get_baseline_info(modelling, base_dir);
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load baseline adjustment: {}\n", e.what());
    }

    try {
        // SES mapping
        // TODO: Maybe this needs its own helper function
        config.ses = get_key(modelling, "ses_model").get<poco::SESInfo>();
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fmt::fg(fmt::color::red), "Could not load SES mappings");
    }

    if (!success) {
        throw ConfigurationError("Could not load modelling info");
    }
}

void load_interventions(const json &j, Configuration &config) {
    const auto interventions = get_key(j, "interventions");

    try {
        // If the type of intervention is null, then there's nothing else to do
        if (interventions.at("active_type_id").is_null()) {
            return;
        }
    } catch (const std::out_of_range &) {
        throw ConfigurationError{"Interventions section missing key \"active_type_id\""};
    }

    core::Identifier active_type_id;
    try {
        active_type_id = interventions["active_type_id"].get<core::Identifier>();
    } catch (const json::type_error &) {
        throw ConfigurationError{"active_type_id key must be of type string"};
    }

    /*
     * NB: This loads all of the policy scenario info from the JSON file, which is
     * strictly speaking unnecessary, but it does mean that we can verify the data
     * format is correct.
     */
    std::unordered_map<core::Identifier, poco::PolicyScenarioInfo> policy_types;
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

void load_running_info(const json &j, Configuration &config) {
    const auto running = get_key(j, "running");

    bool success = true;
    get_to(running, "start_time", config.start_time, success);
    get_to(running, "stop_time", config.stop_time, success);
    get_to(running, "trial_runs", config.trial_runs, success);
    get_to(running, "sync_timeout_ms", config.sync_timeout_ms, success);
    get_to(running, "diseases", config.diseases, success);

    // I copied this logic from the old code, but it seems strange to me. Why do we
    // store multiple seeds but only use the first? -- Alex
    std::vector<unsigned int> seeds;
    if (get_to(running, "seed", seeds, success) && !seeds.empty()) {
        config.custom_seed = seeds[0];
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

bool check_version(const json &j) {
    int version;
    if (!get_to(j, "version", version)) {
        fmt::print(fg(fmt::color::red), "Invalid definition, file must have a schema version");
        return false;
    }

    if (version != 2) {
        fmt::print(fg(fmt::color::red), "Configuration schema version: {} mismatch, supported: 2",
                   version);
        return false;
    }

    return true;
}

auto get_settings(const json &j) {
    if (!j.contains("settings")) {
        fmt::print(fg(fmt::color::red), "\"settings\" key missing");
        throw ConfigurationError{"\"settings\" key missing"};
    }

    return j["settings"].get<poco::SettingsInfo>();
}

void load_inputs(const json &j, const std::filesystem::path &config_dir, Configuration &config) {
    const auto inputs = get_key(j, "inputs");
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

Configuration get_configuration(CommandOptions &options) {
    MEASURE_FUNCTION();
    namespace fs = std::filesystem;
    using namespace host::poco;

    bool success = true;

    Configuration config;
    config.job_id = options.job_id;

    // verbosity
    config.verbosity = core::VerboseMode::none;
    if (options.verbose) {
        config.verbosity = core::VerboseMode::verbose;
    }

    std::ifstream ifs(options.config_file, std::ifstream::in);
    if (!ifs) {
        throw ConfigurationError(
            fmt::format("File {} doesn't exist.", options.config_file.string()));
    }

    const auto opt = [&ifs]() {
        try {
            return json::parse(ifs);
        } catch (const std::exception &e) {
            throw ConfigurationError(fmt::format("Could not parse JSON: {}", e.what()));
        }
    }();

    if (!check_version(opt)) {
        success = false;
    }

    // Base dir for relative paths
    const auto config_dir = options.config_file.parent_path();

    // input dataset file
    try {
        load_inputs(opt, config_dir, config);
        fmt::print("Input dataset file: {}\n", config.file.name.string());
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load dataset file: {}\n", e.what());
    }

    // Modelling information
    try {
        load_modelling_info(opt, config_dir, config);
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load modelling info: {}\n", e.what());
    }

    // Run-time info
    try {
        load_running_info(opt, config);
    } catch (const std::exception &e) {
        success = false;
        fmt::print(fg(fmt::color::red), "Could not load running info: {}\n", e.what());
    }

    if (get_to(opt, "output", config.output, success)) {
        config.output.folder = expand_environment_variables(config.output.folder);
    } else {
        fmt::print(fg(fmt::color::red), "Could not load output info");
    }

    if (!success) {
        throw ConfigurationError{"Error loading config file"};
    }

    return config;
}

std::vector<core::DiseaseInfo> get_diseases_info(core::Datastore &data_api, Configuration &config) {
    std::vector<core::DiseaseInfo> result;
    auto diseases = data_api.get_diseases();
    fmt::print("\nThere are {} diseases in storage, {} selected.\n", diseases.size(),
               config.diseases.size());

    for (const auto &code : config.diseases) {
        result.emplace_back(data_api.get_disease_info(code));
    }

    return result;
}

ModelInput create_model_input(core::DataTable &input_table, core::Country country,
                              Configuration &config, std::vector<core::DiseaseInfo> diseases) {
    // Create simulation configuration
    auto age_range =
        core::IntegerInterval(config.settings.age_range.front(), config.settings.age_range.back());

    auto comorbidities = config.output.comorbidities;
    auto diseases_number = static_cast<unsigned int>(diseases.size());
    if (comorbidities > diseases_number) {
        comorbidities = diseases_number;
        fmt::print(fg(fmt::color::salmon), "Comorbidities value: {}, set to # of diseases: {}.\n",
                   config.output.comorbidities, comorbidities);
    }

    auto settings = Settings(country, config.settings.size_fraction, age_range);
    auto job_custom_seed = create_job_seed(config.job_id, config.custom_seed);
    auto run_info = RunInfo{
        .start_time = config.start_time,
        .stop_time = config.stop_time,
        .sync_timeout_ms = config.sync_timeout_ms,
        .seed = job_custom_seed,
        .verbosity = config.verbosity,
        .comorbidities = comorbidities,
    };

    auto ses_mapping =
        SESDefinition{.fuction_name = config.ses.function, .parameters = config.ses.parameters};

    auto mapping = std::vector<MappingEntry>();
    for (auto &item : config.modelling.risk_factors) {
        if (item.range.empty()) {
            mapping.emplace_back(item.name, item.level);
        } else {
            auto boundary = hgps::OptionalRange{{item.range[0], item.range[1]}};
            mapping.emplace_back(item.name, item.level, boundary);
        }
    }

    return ModelInput(input_table, settings, run_info, ses_mapping,
                      HierarchicalMapping(std::move(mapping)), diseases);
}

std::string create_output_file_name(const poco::OutputInfo &info, int job_id) {
    namespace fs = std::filesystem;

    fs::path output_folder = expand_environment_variables(info.folder);
    auto tp = std::chrono::system_clock::now();
    auto timestamp_tk = fmt::format("{0:%F_%H-%M-}{1:%S}", tp, tp.time_since_epoch());

    // filename token replacement
    auto file_name = info.file_name;
    std::size_t tk_end = 0;
    auto tk_start = file_name.find_first_of("{", tk_end);
    if (tk_start != std::string::npos) {
        tk_end = file_name.find_first_of("}", tk_start + 1);
        if (tk_end != std::string::npos) {
            auto token_str = file_name.substr(tk_start, tk_end - tk_start + 1);
            if (!core::case_insensitive::equals(token_str, "{TIMESTAMP}")) {
                throw std::logic_error(fmt::format("Unknown output file token: {}", token_str));
            }

            file_name.replace(tk_start, tk_end - tk_start + 1, timestamp_tk);
        }
    }

    auto log_file_name = fmt::format("HealthGPS_result_{}.json", timestamp_tk);
    if (tk_end > 0) {
        log_file_name = file_name;
    }

    if (job_id > 0) {
        tk_start = log_file_name.find_last_of(".");
        if (tk_start != std::string::npos) {
            log_file_name.replace(tk_start, size_t{1}, fmt::format("_{}.", std::to_string(job_id)));
        } else {
            log_file_name.append(fmt::format("_{}.json", std::to_string(job_id)));
        }
    }

    log_file_name = (output_folder / log_file_name).string();
    fmt::print(fg(fmt::color::yellow_green), "Output file: {}.\n", log_file_name);
    return log_file_name;
}

ResultFileWriter create_results_file_logger(const Configuration &config,
                                            const hgps::ModelInput &input) {
    return ResultFileWriter{create_output_file_name(config.output, config.job_id),
                            ExperimentInfo{.model = config.app_name,
                                           .version = config.app_version,
                                           .intervention = config.intervention.identifier,
                                           .job_id = config.job_id,
                                           .seed = input.seed().value_or(0u)}};
}

std::unique_ptr<hgps::Scenario> create_baseline_scenario(hgps::SyncChannel &channel) {
    return std::make_unique<BaselineScenario>(channel);
}

hgps::HealthGPS create_baseline_simulation(hgps::SyncChannel &channel,
                                           hgps::SimulationModuleFactory &factory,
                                           hgps::EventAggregator &event_bus,
                                           hgps::ModelInput &input) {
    auto baseline_rnd = std::make_unique<hgps::MTRandom32>();
    auto baseline_scenario = create_baseline_scenario(channel);
    return HealthGPS{
        SimulationDefinition{input, std::move(baseline_scenario), std::move(baseline_rnd)}, factory,
        event_bus};
}

hgps::HealthGPS create_intervention_simulation(hgps::SyncChannel &channel,
                                               hgps::SimulationModuleFactory &factory,
                                               hgps::EventAggregator &event_bus,
                                               hgps::ModelInput &input,
                                               const poco::PolicyScenarioInfo &info) {
    auto policy_scenario = create_intervention_scenario(channel, info);
    auto policy_rnd = std::make_unique<hgps::MTRandom32>();
    return HealthGPS{SimulationDefinition{input, std::move(policy_scenario), std::move(policy_rnd)},
                     factory, event_bus};
}

std::unique_ptr<hgps::InterventionScenario>
create_intervention_scenario(SyncChannel &channel, const poco::PolicyScenarioInfo &info) {
    using namespace hgps;

    fmt::print(fg(fmt::color::light_coral), "\nIntervention policy: {}.\n\n", info.identifier);
    auto period = PolicyInterval(info.active_period.start_time, info.active_period.finish_time);
    auto risk_impacts = std::vector<PolicyImpact>{};
    for (auto &item : info.impacts) {
        risk_impacts.emplace_back(PolicyImpact{core::Identifier{item.risk_factor},
                                               item.impact_value, item.from_age, item.to_age});
    }

    // TODO: Validate intervention JSON definitions!!!
    if (info.identifier == "simple") {
        auto impact_type = PolicyImpactType::absolute;
        if (core::case_insensitive::equals(info.impact_type, "relative")) {
            impact_type = PolicyImpactType::relative;
        } else if (!core::case_insensitive::equals(info.impact_type, "absolute")) {
            throw std::logic_error(fmt::format("Unknown policy impact type: {}", info.impact_type));
        }

        auto definition = SimplePolicyDefinition(impact_type, risk_impacts, period);
        return std::make_unique<SimplePolicyScenario>(channel, std::move(definition));
    }

    if (info.identifier == "marketing") {
        auto definition = MarketingPolicyDefinition(period, risk_impacts);
        return std::make_unique<MarketingPolicyScenario>(channel, std::move(definition));
    }

    if (info.identifier == "dynamic_marketing") {
        auto dynamic = PolicyDynamic{info.dynamics};
        auto definition = MarketingDynamicDefinition{period, risk_impacts, dynamic};
        return std::make_unique<MarketingDynamicScenario>(channel, std::move(definition));
    }

    if (info.identifier == "food_labelling") {
        // I'm not sure if this is safe or not, but I'm supressing the warnings anyway -- Alex
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        const auto cutoff_time = info.coverage_cutoff_time.value();
        const auto cutoff_age = info.child_cutoff_age.value();
        // NOLINTEND(bugprone-unchecked-optional-access)

        auto &adjustment = info.adjustments.at(0);
        auto definition = FoodLabellingDefinition{
            .active_period = period,
            .impacts = risk_impacts,
            .adjustment_risk_factor = AdjustmentFactor{adjustment.risk_factor, adjustment.value},
            .coverage = PolicyCoverage{info.coverage_rates, cutoff_time},
            .transfer_coefficient = TransferCoefficient{info.coefficients, cutoff_age}};

        return std::make_unique<FoodLabellingScenario>(channel, std::move(definition));
    }

    if (info.identifier == "physical_activity") {
        auto definition = PhysicalActivityDefinition{.active_period = period,
                                                     .impacts = risk_impacts,
                                                     .coverage_rate = info.coverage_rates.at(0)};

        return std::make_unique<PhysicalActivityScenario>(channel, std::move(definition));
    }

    if (info.identifier == "fiscal") {
        auto impact_type = parse_fiscal_impact_type(info.impact_type);
        auto definition = FiscalPolicyDefinition(impact_type, period, risk_impacts);
        return std::make_unique<FiscalPolicyScenario>(channel, std::move(definition));
    }

    throw std::invalid_argument(
        fmt::format("Unknown intervention policy identifier: {}", info.identifier));
}

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif
std::string expand_environment_variables(const std::string &path) {
    if (path.find("${") == std::string::npos) {
        return path;
    }

    std::string pre = path.substr(0, path.find("${"));
    std::string post = path.substr(path.find("${") + 2);
    if (post.find('}') == std::string::npos) {
        return path;
    }

    std::string variable = post.substr(0, post.find('}'));
    std::string value = "";

    post = post.substr(post.find('}') + 1);
    const char *v = std::getenv(variable.c_str()); // C4996, but safe here.
    if (v != NULL)
        value = std::string(v);

    return expand_environment_variables(pre + value + post);
}

std::optional<unsigned int> create_job_seed(int job_id, std::optional<unsigned int> user_seed) {
    if (job_id > 0 && user_seed.has_value()) {
        auto rnd = hgps::MTRandom32{user_seed.value()};
        auto jump_size = static_cast<unsigned long>(1.618 * job_id * std::pow(2, 16));
        rnd.discard(jump_size);
        return rnd();
    }

    return user_seed;
}
} // namespace host