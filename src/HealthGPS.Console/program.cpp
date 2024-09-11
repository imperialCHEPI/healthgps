#include "HealthGPS.Core/thread_util.h"
#include "HealthGPS.Input/api.h"
#include "HealthGPS.Input/configuration.h"
#include "HealthGPS.Input/csvparser.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/api.h"
#include "HealthGPS/event_bus.h"
#include "command_options.h"
#include "event_monitor.h"
#include "model_info.h"
#include "result_file_writer.h"

#include <fmt/chrono.h>
#include <fmt/color.h>

#include <chrono>
#include <cstdlib>
#include <oneapi/tbb/global_control.h>

namespace {
/// @brief Get a string representation of current system time
/// @return The system time as string
std::string get_time_now_str() {
    auto tp = std::chrono::system_clock::now();
    return fmt::format("{0:%F %H:%M:}{1:%S} {0:%Z}", tp, tp.time_since_epoch());
}

/// @brief Prints application start-up messages
void print_app_title() {
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
               "\n# Health-GPS Microsimulation for Policy Options #\n\n");

    fmt::print("Today: {}\nMaximum threads: {}\n\n", get_time_now_str(),
               tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism));
}

hgps::ResultFileWriter create_results_file_logger(const hgps::input::Configuration &config,
                                                  const hgps::ModelInput &input) {
    return {create_output_file_name(config.output, config.job_id),
            hgps::ExperimentInfo{.model = config.app_name,
                                 .version = config.app_version,
                                 .intervention = config.active_intervention
                                                     ? config.active_intervention->identifier
                                                     : "",
                                 .job_id = config.job_id,
                                 .seed = input.seed().value_or(0u)}};
}

/// @brief Prints application exit message
/// @param exit_code The application exit code
/// @return The respective exit code
int exit_application(int exit_code) {
    fmt::print("\n\n");
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Goodbye.");
    fmt::print(" {}.\n\n", get_time_now_str());
    return exit_code;
}
} // anonymous namespace

/// @brief Health-GPS host application entry point
/// @param argc The number of command arguments
/// @param argv The list of arguments provided
/// @return The application exit code
int main(int argc, char *argv[]) { // NOLINT(bugprone-exception-escape)
    using namespace hgps;
    using namespace hgps::input;

    // Set thread limit from OMP_THREAD_LIMIT, if set in environment.
    char *env_threads = std::getenv("OMP_THREAD_LIMIT");
    int threads =
        env_threads != nullptr ? std::atoi(env_threads) : tbb::this_task_arena::max_concurrency();
    auto thread_control =
        tbb::global_control(tbb::global_control::max_allowed_parallelism, threads);

    // Create CLI options and validate minimum arguments
    auto options = create_options();
    if (argc < 2) {
        std::cout << options.help() << '\n';
        return exit_application(EXIT_FAILURE);
    }

    // Print application title and parse command line arguments
    print_app_title();

    std::optional<CommandOptions> cmd_args_opt;
    try {
        cmd_args_opt = parse_arguments(options, argc, argv);

        // We won't get a config if e.g. the user chooses the --help option
        if (!cmd_args_opt) {
            return exit_application(EXIT_SUCCESS);
        }
    } catch (const std::exception &ex) {
        fmt::print(fg(fmt::color::red), "\nInvalid command line argument: {}\n", ex.what());
        fmt::print("\n{}\n", options.help());
        return exit_application(EXIT_FAILURE);
    }

    const auto &cmd_args = cmd_args_opt.value();

    // Parse inputs configuration file, *.json.
    Configuration config;
    try {
        config = get_configuration(cmd_args.config_file, cmd_args.output_folder, cmd_args.job_id,
                                   cmd_args.verbose);
    } catch (const std::exception &ex) {
        fmt::print(fg(fmt::color::red), "\n\nInvalid configuration - {}.\n", ex.what());
        return exit_application(EXIT_FAILURE);
    }

    // Create output folder
    if (!std::filesystem::exists(config.output.folder)) {
        fmt::print(fg(fmt::color::dark_salmon), "\nCreating output folder: {} ...\n",
                   config.output.folder);
        if (!std::filesystem::create_directories(config.output.folder)) {
            fmt::print(fg(fmt::color::red), "Failed to create output folder: {}\n",
                       config.output.folder);
            return exit_application(EXIT_FAILURE);
        }
    }

    // Load input data file into a datatable asynchronous
    auto table_future = core::run_async(load_datatable_from_csv, config.file);

#ifdef CATCH_EXCEPTIONS
    try {
#endif
        // In future, we want users to supply the data source via the config file only, but for now
        // we also allow passing it via a command line argument. Sanity check: Make sure they only
        // do one of these things!
        if (cmd_args.data_source.has_value() == (config.data_source != nullptr)) {
            fmt::print(
                fg(fmt::color::red),
                "Must provide a data source via config file or command line, but not both\n");
            return exit_application(EXIT_FAILURE);
        }

        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        const auto &data_source =
            cmd_args.data_source.has_value() ? *cmd_args.data_source : *config.data_source;
        // NOLINTEND(bugprone-unchecked-optional-access)

        // Create back-end data store, cached data repository wrapper
        auto data_api = input::DataManager(data_source.get_data_directory(), config.verbosity);
        auto data_repository = hgps::CachedRepository{data_api};

        // Register the input risk factors model definitions
        register_risk_factor_model_definitions(data_repository, config);

        // Compose Health-GPS with the required modules instances
        auto factory = get_default_simulation_module_factory(data_repository);

        // Validate the configuration's target country for the simulation
        auto country = data_api.get_country(config.settings.country);
        fmt::print("Target country: {} - {}, population: {:0.3g}%.\n", country.code, country.name,
                   config.settings.size_fraction * 100.0f);

        // Validate the configuration diseases list, must exists in back-end data store
        auto diseases = get_diseases_info(data_api, config);
        if (diseases.size() != config.diseases.size()) {
            fmt::print(fg(fmt::color::red), "\nInvalid list of diseases in configuration.\n");
            return exit_application(EXIT_FAILURE);
        }

        // Request input datatable instance, wait, if not completed.
        auto input_table = table_future.get();

        // Print out the datatable structure information
        std::cout << input_table;

        // Create complete model input from configuration
        auto model_input =
            create_model_input(input_table, std::move(country), config, std::move(diseases));

        // Create event bus and event monitor with a results file writer
        auto event_bus = DefaultEventBus();
        auto json_file_logger = create_results_file_logger(config, model_input);
        auto event_monitor = EventMonitor{event_bus, json_file_logger};

        // Create simulation executive instance with master seed generator
        auto seed_generator = std::make_unique<hgps::MTRandom32>();
        if (const auto seed = model_input.seed()) {
            seed_generator->seed(seed.value());
        }
        auto runner = Runner(event_bus, std::move(seed_generator));

        // Create communication channel, shared between the two scenarios.
        auto channel = SyncChannel{};

        // Create simulation engine for each scenario, baseline is always simulated.
        auto runtime = 0.0;
        fmt::print(fg(fmt::color::cyan), "\nStarting baseline simulation with {} trials ...\n\n",
                   config.trial_runs);
        auto baseline_sim = create_baseline_simulation(channel, factory, event_bus, model_input);
        if (config.active_intervention.has_value()) {
            fmt::print(fg(fmt::color::cyan),
                       "\nStarting intervention simulation with {} trials ...\n",
                       config.trial_runs);
            auto policy_sim = create_intervention_simulation(
                channel, factory, event_bus, model_input, config.active_intervention.value());
            runtime = runner.run(baseline_sim, policy_sim, config.trial_runs);
        } else {
            // Baseline only, close channel not store any message
            channel.close();
            runtime = runner.run(baseline_sim, config.trial_runs);
        }

        fmt::print(fg(fmt::color::light_green), "\nCompleted, elapsed time : {}ms\n\n", runtime);
        event_monitor.stop();

#ifdef CATCH_EXCEPTIONS
    } catch (const std::exception &ex) {
        fmt::print(fg(fmt::color::red), "\n\nFailed with message: {}.\n\n", ex.what());

        // Rethrow exception so it can be handled by OS's default handler
        throw;
    }
#endif // CATCH_EXCEPTIONS

    return exit_application(EXIT_SUCCESS);
}

// NOLINTBEGIN(modernize-concat-nested-namespaces)
/// @brief Top-level namespace for Health-GPS Console host application
namespace hgps {
/// @brief Internal details namespace for private data types and functions
namespace detail {}
} // namespace hgps
// NOLINTEND(modernize-concat-nested-namespaces)
