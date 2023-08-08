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

std::string get_time_now_str() {
    auto tp = std::chrono::system_clock::now();
    return fmt::format("{0:%F %H:%M:}{1:%S} {0:%Z}", tp, tp.time_since_epoch());
}

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

void print_app_title() {
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
               "\n# Health-GPS Microsimulation for Policy Options #\n\n");

    fmt::print("Today: {}\n\n", get_time_now_str());
}

CommandOptions parse_arguments(cxxopts::Options &options, int &argc, char *argv[]) {
    MEASURE_FUNCTION();
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
                fmt::print("Configuration file: {}\n", cmd.config_file.string());
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

Configuration load_configuration(CommandOptions &options) {
    MEASURE_FUNCTION();
    namespace fs = std::filesystem;
    using namespace host::poco;

    Configuration config;
    fs::path file_path;
    std::ifstream ifs(options.config_file, std::ifstream::in);

    if (ifs) {
        auto opt = json::parse(ifs);
        if (!opt.contains("version")) {
            throw std::runtime_error("Invalid definition, file must have a schema version");
        }

        auto version = opt["version"].get<int>();
        if (version != 1 && version != 2) {
            throw std::runtime_error(fmt::format(
                "configuration schema version: {} mismatch, supported: 1 and 2", version));
        }

        // application version
        config.app_name = PROJECT_NAME;
        config.app_version = PROJECT_VERSION;

        // Configuration root path
        config.root_path = options.config_file.parent_path();

        // input dataset file
        auto dataset_key = "dataset";
        if (version == 1) {
            dataset_key = "file";
        }

        config.data_file = opt["inputs"][dataset_key].get<DataFileInfo>();
        file_path = config.data_file.name;
        if (file_path.is_relative()) {
            file_path = config.root_path / file_path;
            config.data_file.name = file_path.string();
        }

        fmt::print("Input dataset file: {}\n", config.data_file.name);
        if (!fs::exists(file_path)) {
            fmt::print(fg(fmt::color::red), "\nInput data file: {} not found.\n",
                       file_path.string());
        }

        // Settings and SES mapping
        config.settings = opt["inputs"]["settings"].get<SettingsInfo>();
        config.ses = opt["modelling"]["ses_model"].get<SESInfo>();

        // Modelling information
        config.modelling = opt["modelling"].get<ModellingInfo>();
        for (auto &model : config.modelling.risk_factor_models) {
            file_path = model.second;
            if (file_path.is_relative()) {
                file_path = config.root_path / file_path;
                model.second = file_path.string();
            }

            fmt::print("Risk factor model: {}, file: {}\n", model.first, model.second);
            if (!fs::exists(file_path)) {
                fmt::print(fg(fmt::color::red), "Risk factor model: {}, file: {} not found.\n",
                           model.first, file_path.string());
            }
        }

        for (auto &item : config.modelling.baseline_adjustment.file_names) {
            file_path = item.second;
            if (file_path.is_relative()) {
                file_path = config.root_path / file_path;
                item.second = file_path.string();
            }

            fmt::print("Baseline factor adjustment type: {}, file: {}\n", item.first,
                       file_path.string());
            if (!fs::exists(file_path)) {
                fmt::print(fg(fmt::color::red),
                           "Baseline factor adjustment type: {}, file: {} not found.\n", item.first,
                           file_path.string());
            }
        }

        // Run-time
        opt["running"]["start_time"].get_to(config.start_time);
        opt["running"]["stop_time"].get_to(config.stop_time);
        opt["running"]["trial_runs"].get_to(config.trial_runs);
        opt["running"]["sync_timeout_ms"].get_to(config.sync_timeout_ms);
        auto seed = opt["running"]["seed"].get<std::vector<unsigned int>>();
        if (seed.size() > 0) {
            config.custom_seed = seed[0];
        }

        opt["running"]["diseases"].get_to(config.diseases);

        // Intervention Policy
        auto &interventions = opt["running"]["interventions"];
        if (!interventions["active_type_id"].is_null()) {
            auto active_type = interventions["active_type_id"].get<std::string>();
            auto &policy_types = interventions["types"];
            for (auto it = policy_types.begin(); it != policy_types.end(); ++it) {
                if (core::case_insensitive::equals(it.key(), active_type)) {
                    config.intervention = it.value().get<PolicyScenarioInfo>();
                    config.intervention.identifier = core::to_lower(it.key());
                    config.has_active_intervention = true;
                    break;
                }
            }

            if (!core::case_insensitive::equals(config.intervention.identifier, active_type)) {
                throw std::runtime_error(
                    fmt::format("Unknown active intervention type identifier: {}", active_type));
            }
        }

        config.job_id = options.job_id;
        config.output = opt["output"].get<OutputInfo>();
        config.output.folder = expand_environment_variables(config.output.folder);
        if (!fs::exists(config.output.folder)) {
            fmt::print(fg(fmt::color::dark_salmon), "\nCreating output folder: {} ...\n",
                       config.output.folder);
            if (!create_output_folder(config.output.folder)) {
                throw std::runtime_error(
                    fmt::format("Failed to create output folder: {}", config.output.folder));
            }
        }

        // verbosity
        config.verbosity = core::VerboseMode::none;
        if (options.verbose) {
            config.verbosity = core::VerboseMode::verbose;
        }
    } else {
        std::cout << fmt::format("File {} doesn't exist.", options.config_file.string())
                  << std::endl;
    }

    ifs.close();
    return config;
}

bool create_output_folder(std::filesystem::path folder_path, unsigned int num_retries) {
    using namespace std::chrono_literals;
    for (unsigned int i = 1; i <= num_retries; i++) {
        try {
            if (std::filesystem::create_directories(folder_path)) {
                return true;
            }
        } catch (const std::exception &ex) {
            fmt::print(fg(fmt::color::red), "Failed to create output folder, attempt #{} - {}.\n",
                       i, ex.what());
        }

        std::this_thread::sleep_for(1000ms);
    }

    return false;
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