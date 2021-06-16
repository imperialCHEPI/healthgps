#include "program.h"
#include "csvparser.h"
#include "HealthGPS.Datastore/api.h"

#include <fmt/chrono.h>
#include <adevs/adevs.h>

using namespace hgps;
namespace fs = std::filesystem;

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

	auto input_table = hgps::data::DataTable();
	if (!load_csv(config.file.name, config.file.columns, input_table, config.file.delimiter))
	{
		return EXIT_FAILURE;
	}

	std::cout << input_table.to_string();

	// Create infrastructure
	auto scenario = create_scenario(config);
	auto data_api = hgps::data::DataManager(settings.storage_folder);
	auto factory = hgps::ModuleFactory(data_api);

	// Validate target country
	auto countries = data_api.get_countries();
	fmt::print("\nThere are {} countries in storage.\n", countries.size());
	auto target = find_country(countries, scenario.country);
	if (target.has_value())	{
		fmt::print("Target country: {} - {}.\n", target.value().code, target.value().name);
	}
	else {
		fmt::print(fg(fmt::color::light_salmon), "Target country: {} not found.\n", scenario.country);
	}

	try
	{
		// Create model
		auto model = hgps::HealthGPS(scenario, hgps::MTRandom32());

		fmt::print(fg(fmt::color::cyan), "\nStarting simulation ...\n\n");
		auto runner = hgps::ModelRunner(model, factory);
		auto runtime = runner.run();
		fmt::print(fg(fmt::color::light_green), "Completed, elapsed time : {}ms", runtime);
	}
	catch (const std::exception& ex)
	{
		fmt::print(fg(fmt::color::red), "\n\nFailed with message {}.\n", ex.what());
	}

	fmt::print("\n\n");
	fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Goodbye");
	fmt::print("\n\n");

	return EXIT_SUCCESS;
}
