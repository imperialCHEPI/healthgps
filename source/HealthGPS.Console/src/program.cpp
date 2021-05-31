#include "program.h"

#include "../../HealthGPS/adevs/adevs.h"

using namespace hgps::host;

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

	if (result.count("file"))
	{
		auto file_name = result["file"].as<std::string>();
		auto config = create_json();
		fmt::print("Configuration file: {}\n{}\n", file_name, config.dump(4));
	}

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
	
	/* Create the model */
	hgps::Scenario inputs(2010, 2030);

	//auto model = hgps::Simulation(inputs, hgps::MTRandom32());
	auto model = hgps::Simulation(inputs);

	/* Create the simulation context */
	adevs::Simulator<int> sim;

	/* Add the model to the context */
	sim.add(&model);

	/* Run until the next event is at infinity */
	while (sim.next_event_time() < adevs_inf<adevs::Time>()) {
		sim.exec_next_event();
	}

	return EXIT_SUCCESS;
}

