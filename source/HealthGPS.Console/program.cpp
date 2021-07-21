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

	auto cmd_args = parse_arguments(options, argc, argv);
	if (!cmd_args.success)
	{
		return cmd_args.exit_code;
	}

	// Parse configuration file 
	auto config = load_configuration(cmd_args);
	auto input_table = core::DataTable();
	if (!load_csv(config.file.name, config.file.columns, input_table, config.file.delimiter))
	{
		return EXIT_FAILURE;
	}

	std::cout << input_table.to_string();

	// Load risk factors model definition
	auto risk_factor_module = build_risk_factor_module(config.modelling);

	// Create infrastructure
	auto data_api = data::DataManager(cmd_args.storage_folder);
	auto factory = SimulationModuleFactory(data_api);
	factory.register_builder(SimulationModuleType::SES,
		[](core::Datastore& manager, ModelInput& config) -> SimulationModuleFactory::ModuleType {
			return build_ses_module(manager, config);});
	factory.register_builder(SimulationModuleType::Demographic,
		[](core::Datastore& manager, ModelInput& config) -> SimulationModuleFactory::ModuleType {
			return build_demographic_module(manager, config);});
	factory.register_instance(SimulationModuleType::RiskFactor, risk_factor_module);

	// Validate target country
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

	// Validate diseases
	auto diseases = get_diseases(data_api, config);
	if (diseases.size() != config.diseases.size()) {
		fmt::print(fg(fmt::color::light_salmon), "Invalid list of diseases in configuration.\n");
		return EXIT_FAILURE;
	}

	// Create model configuration
	auto model_config = create_model_input(input_table, target.value(), config, diseases);

	try	{
		// Create model
		auto model = HealthGPS(factory, model_config, hgps::MTRandom32());

		fmt::print(fg(fmt::color::cyan), "\nStarting simulation ...\n\n");
		auto runner = ModelRunner();
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
