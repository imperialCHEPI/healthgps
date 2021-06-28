#include "program.h"
#include "csvparser.h"
#include "HealthGPS.Datastore/api.h"

#include <fmt/chrono.h>

int main(int argc, char* argv[])
{
	// Parse command line arguments
	auto options = create_options();
	if (argc < 2) {
		std::cout << options.help() << std::endl;
		return EXIT_FAILURE;
	}

	fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
		"\n# Health-GPS Microsimulation for Policy Options #\n");

	fmt::print("\nToday: {}\n\n", getTimeNowStr());

	auto settings = parse_arguments(options, argc, argv);
	if (!settings.success)
	{
		return settings.exit_code;
	}

	// Parse configuration file 
	auto config = load_configuration(settings);
	auto input_table = core::DataTable();
	if (!load_csv(config.file.name, config.file.columns, input_table, config.file.delimiter))
	{
		return EXIT_FAILURE;
	}

	std::cout << input_table.to_string();

	// Create infrastructure
	auto scenario = create_scenario(config);
	auto data_api = data::DataManager(settings.storage_folder);
	auto factory = SimulationModuleFactory(data_api);
	factory.Register(SimulationModuleType::SES,
		[](core::Datastore& manager, ModelContext& context) -> SimulationModuleFactory::ModuleType {
			return build_ses_module(manager, context);});
	factory.Register(SimulationModuleType::Demographic,
		[](core::Datastore& manager, ModelContext& context) -> SimulationModuleFactory::ModuleType {
			return build_demographic_module(manager, context);});

	// Validate target country
	auto countries = data_api.get_countries();
	fmt::print("There are {} countries in storage.\n", countries.size());
	auto target = data_api.get_country(scenario.country);
	if (target.has_value())	{
		fmt::print("Target country: {} - {}.\n", target.value().code, target.value().name);
	}
	else {
		fmt::print(fg(fmt::color::light_salmon), "Target country: {} not found.\n", scenario.country);
		return EXIT_FAILURE;
	}

	// Create model context
	auto context = create_context(input_table, target.value(), config);

	try	{
		// auto ses_module = factory.Create(SimulationModuleType::SES, context);

		// Create model
		auto model = HealthGPS(context, hgps::MTRandom32());

		fmt::print(fg(fmt::color::cyan), "\nStarting simulation ...\n\n");
		auto runner = ModelRunner(factory, context);

		auto runtime = runner.run(model, config.trial_runs);
		fmt::print(fg(fmt::color::light_green), "Completed, elapsed time : {}ms", runtime);
	}
	catch (const std::exception& ex) {
		fmt::print(fg(fmt::color::red), "\n\nFailed with message {}.\n", ex.what());
	}

	fmt::print("\n\n");
	fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Goodbye");
	fmt::print("\n\n");

	return EXIT_SUCCESS;
}
