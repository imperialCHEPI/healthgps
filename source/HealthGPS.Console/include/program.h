#ifndef HEALTHGPS_CONSOLE_HPP_INCLUDED
#define HEALTHGPS_CONSOLE_HPP_INCLUDED

#include <chrono>
#include <ctime>
#include <iostream>
#include <fmt/core.h>
#include <fmt/color.h>
#include <cxxopts.hpp>
#include <nlohmann/json.hpp>

namespace hgps {
	namespace host {

		using json = nlohmann::json;

		std::string getTimeNowStr() {
			std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::string s(30, '\0');

			struct tm localtime;

			localtime_s(&localtime, &now);

			std::strftime(&s[0], s.size(), "%c", &localtime);

			return s;
		}

		cxxopts::Options create_options()
		{
			cxxopts::Options options("C++20 Study", "Creates a modern C++ tool chain study.");
			options.add_options()
				("h,help", "Help about this application.")
				("f,file", "File name", cxxopts::value<std::string>())
				("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"));

			return options;
		}

		json create_json()
		{
			json j2 = {
				{"pi", 3.141},
				{"happy", true},
				{"name", "Boris"},
				{"nothing", nullptr},
				{"answer", {
					{"everything", 13}
				}},
				{"list", {1, 3, 7, 0}},
				{"object", {
					{"currency", "GBP"},
					{"value", 75.13}
				}}
			};

			return j2;
		}

		template<typename Gen>
		concept random_number_engine = std::uniform_random_bit_generator<Gen>
			&& requires (Gen rnd)
		{
			{rnd.seed()};
			{rnd.seed(1U)};
			{rnd.discard(1ULL)};
		};

		template<random_number_engine Gen>
		auto rand_gen(Gen&& rnd, std::optional<unsigned int> seed = std::nullopt)
		{
			if (seed.has_value())
			{
				rnd.seed(seed.value());
			}

			return rnd(); // std::generate_canonical<double, 10>(rnd);
		}
	}
}

#endif //HEALTHGPS_CONSOLE_HPP_INCLUDED