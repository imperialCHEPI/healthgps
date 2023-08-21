#include "configuration.h"
#include "csvparser.h"
#include "model_parser.h"

#include "HealthGPS.Core/thread_util.h"
#include "HealthGPS.Datastore/api.h"
#include "HealthGPS/api.h"
#include "HealthGPS/event_bus.h"
#include "event_monitor.h"

#include <fmt/chrono.h>
#include <fmt/color.h>

#include <chrono>

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

    fmt::print("Today: {}\n\n", get_time_now_str());
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

/// @brief Health-GPS host application entry point
/// @param argc The number of command arguments
/// @param argv The list of arguments provided
/// @return The application exit code
int main(int argc, char *argv[]) { // NOLINT(bugprone-exception-escape)
    using namespace hgps;
    using namespace host;

    // Create CLI options and validate minimum arguments
    auto options = create_options();
    if (argc < 2) {
        std::cout << options.help() << std::endl;
        return exit_application(EXIT_FAILURE);
    }

    // Print application title and parse command line arguments
    print_app_title();
    auto cmd_args = parse_arguments(options, argc, argv);
    if (!cmd_args.success) {
        return cmd_args.exit_code;
    }

    // Parse inputs configuration file, *.json.
    Configuration config;
    try {
        config = get_configuration(cmd_args);
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
        // Create back-end data store, cached data repository wrapper
        auto data_api = data::DataManager(cmd_args.storage_folder, config.verbosity);
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
        auto model_input = create_model_input(input_table, country, config, diseases);

        // Create event bus and event monitor with a results file writer
        auto event_bus = DefaultEventBus();
        auto json_file_logger = create_results_file_logger(config, model_input);
        auto event_monitor = EventMonitor{event_bus, json_file_logger};

        // Create simulation executive instance with master seed generator
        auto seed_generator = std::make_unique<hgps::MTRandom32>();
        if (const auto seed = model_input.seed()) {
            seed_generator->seed(seed.value());
        }
        auto executive = ModelRunner(event_bus, std::move(seed_generator));

        // Create communication channel, shared between the two scenarios.
        auto channel = SyncChannel{};

        // Create simulation engine for each scenario, baseline is always simulated.
        auto runtime = 0.0;
        std::atomic<bool> done(false);
        fmt::print(fg(fmt::color::cyan), "\nStarting baseline simulation with {} trials ...\n\n",
                   config.trial_runs);
        auto baseline_sim = create_baseline_simulation(channel, factory, event_bus, model_input);
        if (config.active_intervention.has_value()) {
            fmt::print(fg(fmt::color::cyan),
                       "\nStarting intervention simulation with {} trials ...\n",
                       config.trial_runs);
            auto policy_sim = create_intervention_simulation(
                channel, factory, event_bus, model_input, config.active_intervention.value());

            // Run simulations side by side on a background thread
            auto worker =
                std::jthread{[&runtime, &executive, &baseline_sim, &policy_sim, &config, &done] {
                    runtime = executive.run(baseline_sim, policy_sim, config.trial_runs);
                    done.store(true);
                }};

            // Wait until done or error, before joining the thread
            while (!done.load()) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            worker.join();
        } else {
            // Baseline only, close channel not store any message
            channel.close();

            // Run simulation on a background thread
            auto worker = std::jthread{[&runtime, &executive, &baseline_sim, &config, &done] {
                runtime = executive.run(baseline_sim, config.trial_runs);
                done.store(true);
            }};

            // Wait until done or error, before joining the thread
            while (!done.load()) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            worker.join();
        }

        // Waits for messages queue to be processed before stopping events monitor
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
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

/// @brief Top-level namespace for Health-GPS Console host application
namespace host {
/// @brief Internal details namespace for private data types and functions
namespace detail {}
} // namespace host