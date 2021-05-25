#include <concepts>
#include <random>
#include "boost/ut.hpp"
#include "program.h"

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

	// TDD macro-free framework
	auto value = rand_gen(std::mt19937{});
	boost::ut::expect(value >= 0);

	return EXIT_SUCCESS;
}
