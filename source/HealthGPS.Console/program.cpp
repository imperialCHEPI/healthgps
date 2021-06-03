#include "program.h"
#include <fmt/chrono.h>
#include <adevs/adevs.h>

using namespace hgps::host;

namespace fs = std::filesystem;

void run_experimental();

int main(int argc, char* argv[])
{
	fmt::print(fg(fmt::color::yellow) | bg(fmt::color::blue) |
		fmt::emphasis::bold, "\n# Hello Health-GPS Microsimulation World #\n\n");

	fmt::print("Today: {}\n\n", getTimeNowStr());

	auto options = create_options();
	auto result = options.parse(argc, argv);
	if (result.count("help"))
	{
		std::cout << options.help() << std::endl;
		return EXIT_SUCCESS;
	}

	std::optional<hgps::Scenario> scenario;
	if (result.count("file"))
	{
		fs::path file_name = result["file"].as<std::string>();
		std::string full_path{ file_name.string() };
		if (file_name.is_relative()) {
			full_path = std::filesystem::absolute(file_name).string();
		}

		scenario = create_scenario(full_path);
		if (scenario.has_value()) {
			fmt::print("Configuration file: {}\n", full_path);
		}
	}
		
	/* Create the model */
	if (!scenario.has_value()) {
		fmt::print(fg(fmt::color::blue) | bg(fmt::color::alice_blue),
			       "\nNo configuration file, running default scenario.\n");
		hgps::Scenario inputs(2015, 2025);
	}

	auto model = hgps::Simulation(scenario.value(), hgps::MTRandom32());

	fmt::print(fg(fmt::color::green) | bg(fmt::color::alice_blue),
		"\nStarting simulation ...\n\n");

	auto start = std::chrono::steady_clock::now();
	try
	{
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
		fmt::print(fg(fmt::color::blue) | bg(fmt::color::light_yellow),
			"Completed, elapsed time : {}ms", elapsed.count());
	}
	catch (const std::exception& ex)
	{
		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
		fmt::print(fg(fmt::color::red), "\n\nFailed after {}ms with message {}.\n", elapsed.count(), ex.what());
	}

	fmt::print(fg(fmt::color::light_green), "\n\nGoodbye.\n");

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
