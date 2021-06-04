#include "program.h"
#include <fmt/chrono.h>
#include <adevs/adevs.h>
#include "HealthGPS.Datastore/api.h"

using namespace hgps::host;

namespace fs = std::filesystem;

void run_experimental();

int main(int argc, char* argv[])
{
	fmt::print(fg(fmt::color::yellow) | bg(fmt::color::blue) |
		fmt::emphasis::bold, "\n# Hello Health-GPS Microsimulation World #\n");

	fmt::print("\nToday: {}\n\n", getTimeNowStr());

	auto options = create_options();
	auto result = options.parse(argc, argv);
	if (result.count("help"))
	{
		std::cout << options.help() << std::endl;
		return EXIT_SUCCESS;
	}

	fs::path config_file {};
	if (result.count("file"))
	{
		config_file = result["file"].as<std::string>();
		if (config_file.is_relative()) {
			config_file = std::filesystem::absolute(config_file);
			fmt::print("Configuration file.: {}\n", config_file.string());
		}
	}

	if (!fs::exists(config_file)) {
		fmt::print(fg(fmt::color::red), "\n\nConfiguration file: {} not found.\n", config_file.string());
		return EXIT_FAILURE;
	}

	fs::path storage_folder {};
	if (result.count("storage"))
	{
		storage_folder = result["storage"].as<std::string>();
		if (storage_folder.is_relative()) {
			storage_folder = std::filesystem::absolute(storage_folder);
			fmt::print("File storage folder: {}\n", storage_folder.string());
		}
	}

	if (!fs::exists(storage_folder)) {
		fmt::print(fg(fmt::color::red), "\n\nFile storage folder: {} not found.\n", storage_folder.string());
		return EXIT_FAILURE;
	}

	/* Create the model */
	auto scenario = create_scenario(config_file);
	auto repo = hgps::data::FileRepository{ storage_folder };
	auto data_api = hgps::data::DataManager(repo);
	
	auto model = hgps::HealthGPS(scenario, hgps::MTRandom32());

	fmt::print(fg(fmt::color::cyan), "\nStarting simulation ...\n\n");

	auto start = std::chrono::steady_clock::now();
	try
	{
		// Selected country
		auto countries = data_api.get_countries();
		fmt::print("There are {} countries in storage.\n\n", countries.size());

		/* Create the simulation context */
		adevs::Simulator<int> sim;

		/* Add the model to the context */
		sim.add(&model);

		/* Run until the next event is at infinity */
		while (sim.next_event_time() < adevs_inf<adevs::Time>()) {
			sim.exec_next_event();
		}

		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
		fmt::print("\n");
		fmt::print(fg(fmt::color::light_green), "Completed, elapsed time : {}ms", elapsed.count());
	}
	catch (const std::exception& ex)
	{
		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
		fmt::print(fg(fmt::color::red), "\n\nFailed after {}ms with message {}.\n", elapsed.count(), ex.what());
	}

	fmt::print("\n\n");
	fmt::print(fg(fmt::color::yellow) | bg(fmt::color::blue), "Goodbye");
	fmt::print("\n\n");

	return EXIT_SUCCESS;
}

void run_experimental()
{
	fs::path p = fs::current_path();
	while (p.has_parent_path())
	{
		std::cout << p << '\n';
		if (p == p.root_path()) {
			break;
		}

		p = p.parent_path();
	}

	std::cout << std::endl;

	auto value = rand_gen(std::mt19937{});
	fmt::print("The lucky number is: {}\n\n", value);

	fmt::print("Random bit generator: \n");
	auto seed = 123456789U;
	std::mt19937 mtrnd(seed);
	std::mt19937_64 mt64(seed);

	hgps::MTRandom32 rnd(seed);

	fmt::print("Min #: {} = {} => {}\n", std::mt19937::min(), hgps::MTRandom32::min(), hgps::RandomBitGenerator::min());
	fmt::print("Max #: {} = {} => {}\n", std::mt19937::max(), hgps::MTRandom32::max(), hgps::RandomBitGenerator::max());

	fmt::print("Min #: {} Max: {}\n", std::mt19937_64::min(), std::mt19937_64::max());

	fmt::print("Rnd #: {} = {}\n", mtrnd(), rnd());

	print_uniform_int_dist(rnd, 10, 50);
	print_canonical(rnd, 5);
}
