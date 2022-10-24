#include "configuration.h"
#include "model_parser.h"
#include "csvparser.h"

#include "HealthGPS/api.h"
#include "HealthGPS.Datastore/api.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS.Core/thread_util.h"

#include "event_monitor.h"

#include <fmt/color.h>

int main(int argc, char* argv[])
{
	using namespace hgps;

	// Parse command line arguments
	auto options = create_options();
	if (argc < 2) {
		std::cout << options.help() << std::endl;
		return EXIT_FAILURE;
	}

	print_app_title();
	auto cmd_args = parse_arguments(options, argc, argv);
	if (!cmd_args.success) {
		return cmd_args.exit_code;
	}

	// Parse configuration file
	Configuration config;
	try {
		config = load_configuration(cmd_args);
	}
	catch (const std::exception& ex) {
		fmt::print(fg(fmt::color::red), "\n\nInvalid configuration - {}.\n", ex.what());
		return EXIT_FAILURE;
	}
	
	// load datatable asynchronous 
	auto input_table = core::DataTable();
	auto table_future = core::run_async(load_datatable_csv,
		std::ref(input_table), config.file.name, config.file.columns, config.file.delimiter);

	// Create back-end data store and modules factory infrastructure
	auto data_api = data::DataManager(cmd_args.storage_folder, config.verbosity);
	auto data_repository = hgps::CachedRepository{ data_api };
	register_risk_factor_model_definitions(data_repository, config.modelling, config.settings);
	auto factory = get_default_simulation_module_factory(data_repository);

	// Validate the configuration target country
	auto countries = data_api.get_countries();
	fmt::print("\nThere are {} countries in storage.\n", countries.size());
	auto target = data_api.get_country(config.settings.country);
	if (target.has_value()) {
		fmt::print("Target country: {} - {}, population: {:0.3g}%.\n", 
			target.value().code, target.value().name, config.settings.size_fraction*100.0f);
	} 
	else {
		fmt::print(fg(fmt::color::light_salmon), "Target country: {} not found.\n", config.settings.country);
		return EXIT_FAILURE;
	}

	// Validate the configuration diseases list
	auto diseases = get_diseases(data_api, config);
	if (diseases.size() != config.diseases.size()) {
		fmt::print(fg(fmt::color::light_salmon), "Invalid list of diseases in configuration.\n");
		return EXIT_FAILURE;
	}

	// wait for datatable to complete
	if (!table_future.get()) {
		return EXIT_FAILURE;
	}

	std::cout << input_table;

	// Create the complete model input from configuration
	auto model_input = create_model_input(input_table, target.value(), config, diseases);

	// Create event bus and monitor
	auto event_bus = DefaultEventBus();
	auto json_file_logger = create_results_file_logger(config, model_input);
	auto event_monitor = EventMonitor{ event_bus, json_file_logger };

	try {
		auto seed_generator = std::make_unique<hgps::MTRandom32>();
		if (model_input.seed().has_value()) {
			seed_generator->seed(model_input.seed().value());
		}

		auto executive = ModelRunner(event_bus, std::move(seed_generator));
		auto channel = SyncChannel{};

		// Create main simulation model instance and run experiment
		std::atomic<bool> done(false);
		auto runtime = 0.0;

		// GCC thread initialisation bug reverses derived to base-class, when declared in line
		fmt::print(fg(fmt::color::cyan), "\nStarting baseline simulation with {} trials ...\n\n", config.trial_runs);
		auto baseline_sim = create_baseline_simulation(channel, factory, event_bus, model_input, config.intervention);

		if (config.has_active_intervention) {
			fmt::print(fg(fmt::color::cyan), "\nStarting intervention simulation with {} trials ...\n", config.trial_runs);
			auto policy_sim = create_intervention_simulation(channel, factory, event_bus, model_input, config.intervention);

			auto worker = std::jthread{ [&runtime, &executive, &baseline_sim, &policy_sim, &config, &done] {
				runtime = executive.run(baseline_sim, policy_sim, config.trial_runs);
				done.store(true);
			} };

			while (!done.load()) {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}

			worker.join();
		}
		else {
			channel.close(); // Will not store any message
			auto worker = std::jthread{ [&runtime, &executive, &baseline_sim, &config, &done] {
				runtime = executive.run(baseline_sim, config.trial_runs);
				done.store(true);
			} };

			while (!done.load()) {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}

			worker.join();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		fmt::print(fg(fmt::color::light_green), "\nCompleted, elapsed time : {}ms\n\n", runtime);
	}
	catch (const std::exception& ex) {
		fmt::print(fg(fmt::color::red), "\n\nFailed with message - {}.\n\n", ex.what());
	}

	event_monitor.stop();
	fmt::print("\n\n");
	fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Goodbye.");
	fmt::print(" {}.\n\n", get_time_now_str());

	return EXIT_SUCCESS;
}
