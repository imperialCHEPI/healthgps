#pragma once

#include <chrono>
#include <ctime>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <fstream>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/color.h>
#include <cxxopts.hpp>
#include <nlohmann/json.hpp>

#include "HealthGPS/api.h"

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
				("f,file", "Configuration file full name.", cxxopts::value<std::string>())
				("s,storage","Path to file data storage root folder.", cxxopts::value<std::string>())
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

		Scenario create_scenario(std::filesystem::path file_name)
		{
			std::ifstream ifs(file_name, std::ifstream::in);
			if (ifs)
			{
				auto config = json::parse(ifs);
				auto start = config["running"]["start_time"].get<int>();
				auto stop = config["running"]["stop_time"].get<int>();
				auto seed = config["running"]["seed"].get<std::vector<unsigned int>>();
				Scenario settings(start, stop);
				if (seed.size() > 0) {
					settings.custom_seed = seed[0];
				}

				settings.country = config["inputs"]["population"]["country"].get<std::string>();

				ifs.close();
				return settings;
			}
			else
			{
				std::cout << std::format("File {} doesn't exist.", file_name.string()) << std::endl;
			}

			ifs.close();
			return Scenario(2015, 2025);
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
		};

		/*
		class RandomBitGenerator
		{
		public:
			using result_type = unsigned int;
			virtual result_type operator()() = 0;

			static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
			static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
		};

		class Random : public RandomBitGenerator
		{
		public:
			Random() = delete;
			Random(std::optional<unsigned int> seed = std::nullopt) : RandomBitGenerator()
			{
				if (seed.has_value())
				{
					rnd.seed(seed.value());
				}
				else
				{
					std::random_device rd;
					rnd.seed(rd());
				}
			}

			~Random() = default;

			result_type operator()() override { return rnd(); }

		private:
			std::mt19937 rnd;
		};
		*/

		void print_canonical(RandomBitGenerator& rnd, unsigned int n)
		{
			for (size_t i = 0; i < n; i++) {
				fmt::print("{} ", std::generate_canonical<float, 5>(rnd));
			}

			fmt::print("\n\n");
		}

		void print_uniform_int_dist(RandomBitGenerator& rnd, unsigned int n, int max = 10)
		{
			std::uniform_int_distribution undist(0, max);
			for (size_t i = 0; i < n; i++) {
				fmt::print("{} ", undist(rnd));
			}

			fmt::print("\n\n");
		}
	}
}
