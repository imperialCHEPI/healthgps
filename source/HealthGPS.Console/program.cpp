#include "configuration.h"
#include "model_parser.h"
#include "csvparser.h"

#include "HealthGPS/api.h"
#include "HealthGPS.Datastore/api.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/baseline_scenario.h"
#include "HealthGPS/intervention_scenario.h"

#include "event_monitor.h"
#include "result_file_writer.h"

#include <fmt/color.h>
#include <fmt/chrono.h>
#include <thread>

int main(int argc, char* argv[])
{
	using namespace hgps;

	// Parse command line arguments
	auto options = create_options();
	if (argc < 2) {
		std::cout << options.help() << std::endl;
		return EXIT_FAILURE;
	}

	fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
		"\n# Health-GPS Microsimulation for Policy Options #\n");

	fmt::print("\nToday: {}\n\n", getTimeNowStr());

	auto cmd_args = parse_arguments(options, argc, argv);
	if (!cmd_args.success)
	{
		return cmd_args.exit_code;
	}
	
	// Parse configuration file 
	auto config = load_configuration(cmd_args);
	auto input_table = core::DataTable();
	if (!load_datatable_csv(config.file.name, config.file.columns, input_table, config.file.delimiter))
	{
		return EXIT_FAILURE;
	}

	std::cout << input_table.to_string();

	// Create back-end data store and modules factory infrastructure
	auto data_api = data::DataManager(cmd_args.storage_folder);
	auto data_repository = hgps::CachedRepository(data_api);
	register_risk_factor_model_definitions(config.modelling, data_repository);
	auto factory = get_default_simulation_module_factory(data_repository);

	// Validate the configuration target country
	auto countries = data_api.get_countries();
	fmt::print("\nThere are {} countries in storage.\n", countries.size());
	auto target = data_api.get_country(config.settings.country);
	if (target.has_value())	{
		fmt::print("Target country: {} - {}.\n", target.value().code, target.value().name);
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

	// Create the complete model configuration
	auto model_config = create_model_input(input_table, target.value(), config, diseases);

	// Create event bus and monitor
	auto event_bus = DefaultEventBus();
	auto result_file_logger = ResultFileWriter{ 
		create_output_file_name(config.result),
		ModelInfo{.name = "Health-GPS", .version = "0.6.beta"}
	};
	auto event_monitor = EventMonitor{ event_bus, result_file_logger };

	try	{
		auto seed_generator = hgps::MTRandom32{};
		if (model_config.seed().has_value()) {
			seed_generator.seed(model_config.seed().value());
		}

		auto runner = ModelRunner(event_bus, std::move(seed_generator));
		auto channel = SyncChannel{};

		// Create main simulation model instance and run experiment
		std::atomic<bool> done(false);
		auto runtime = 0.0;
		auto baseline = HealthGPS{
			SimulationDefinition{ model_config, std::make_unique<BaselineScenario>(channel), hgps::MTRandom32() },
			factory, event_bus };
		if (config.has_active_intervention) {
			auto policy_scenario = create_intervention_scenario(channel, config.intervention);
			auto intervention = HealthGPS{
				SimulationDefinition{ model_config, std::move(policy_scenario), hgps::MTRandom32() },
				factory, event_bus };

			fmt::print(fg(fmt::color::cyan), "\nStarting intervention simulation ...\n\n");
			auto worker = std::jthread{ [&runtime, &runner, &baseline, &intervention, &config, &done] {
				runtime = runner.run(baseline, intervention, config.trial_runs);
				done.store(true);
			} };

			while (!done.load()) {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}

			worker.join();
		}
		else {
			fmt::print(fg(fmt::color::cyan), "\nStarting baseline simulation ...\n\n");
			channel.close(); // Will not store any message
			auto worker = std::jthread{ [&runtime, &runner, &baseline, &config, &done ] {
				runtime = runner.run(baseline, config.trial_runs);
				done.store(true);
			} };

			while (!done.load()) {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}

			worker.join();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		fmt::print(fg(fmt::color::light_green), "Completed, elapsed time : {}ms\n\n", runtime);
	}
	catch (const std::exception& ex) {
		fmt::print(fg(fmt::color::red), "\n\nFailed with message - {}.\n", ex.what());
	}

	event_monitor.stop();
	fmt::print("\n\n");
	fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Goodbye");
	fmt::print("\n\n");

	return EXIT_SUCCESS;
}
