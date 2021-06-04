#pragma once

#include <chrono>
#include <ctime>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <fstream>
#include <fmt/core.h>
#include <fmt/color.h>
#include <cxxopts.hpp>
#include <nlohmann/json.hpp>

#include "HealthGPS/api.h"
#include "options.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

std::string getTimeNowStr() {
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::string s(30, '\0');

	struct tm localtime;

	localtime_s(&localtime, &now);

	std::strftime(&s[0], s.size(), "%c", &localtime);

	return s;
}

bool iequals(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(), b.begin(), b.end(),
		[](char a, char b) { return tolower(a) == tolower(b); });
}

cxxopts::Options create_options()
{
	cxxopts::Options options("HealthGPS.Console", "Health-GPS microsimulation for policy options.");
	options.add_options()
		("h,help", "Help about this application.")
		("f,file", "Configuration file full name.", cxxopts::value<std::string>())
		("s,storage", "Path to root folder of the data storage.", cxxopts::value<std::string>())
		("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"));

	return options;
}

CommandOptions parse_arguments(cxxopts::Options& options, int& argc, char* argv[])
{
	CommandOptions cmd;
	try
	{
		cmd.success = true;
		cmd.exit_code = EXIT_SUCCESS;
		auto result = options.parse(argc, argv);
		if (result.count("help"))
		{
			std::cout << options.help() << std::endl;
			cmd.success = false;
			return cmd;
		}

		if (result.count("file"))
		{
			cmd.config_file = result["file"].as<std::string>();
			if (cmd.config_file.is_relative()) {
				cmd.config_file = std::filesystem::absolute(cmd.config_file);
				fmt::print("Configuration file.: {}\n", cmd.config_file.string());
			}
		}

		if (!fs::exists(cmd.config_file)) {
			fmt::print(fg(fmt::color::red),
				"\n\nConfiguration file: {} not found.\n",
				cmd.config_file.string());
			cmd.exit_code = EXIT_FAILURE;
		}

		if (result.count("storage"))
		{
			cmd.storage_folder = result["storage"].as<std::string>();
			if (cmd.storage_folder.is_relative()) {
				cmd.storage_folder = std::filesystem::absolute(cmd.storage_folder);
				fmt::print("File storage folder: {}\n", cmd.storage_folder.string());
			}
		}

		if (!fs::exists(cmd.storage_folder)) {
			fmt::print(fg(fmt::color::red),
				"\n\nFile storage folder: {} not found.\n",
				cmd.storage_folder.string());
			cmd.exit_code = EXIT_FAILURE;
		}

		cmd.success = cmd.exit_code == EXIT_SUCCESS;
	}
	catch (const cxxopts::OptionException& ex) {
		fmt::print(fg(fmt::color::red), "\nInvalid command line argument: {}.\n", ex.what());
		fmt::print("\n{}\n", options.help());
		cmd.success = false;
		cmd.exit_code = EXIT_FAILURE;
	}

	return cmd;
}

hgps::Scenario create_scenario(std::filesystem::path file_name)
{
	std::ifstream ifs(file_name, std::ifstream::in);
	if (ifs)
	{
		auto config = json::parse(ifs);
		auto start = config["running"]["start_time"].get<int>();
		auto stop = config["running"]["stop_time"].get<int>();
		auto seed = config["running"]["seed"].get<std::vector<unsigned int>>();

		hgps::Scenario settings(start, stop);
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

	// default test scenario
	return hgps::Scenario(2015, 2025);
}


std::optional<hgps::core::Country> find_country(const std::vector<hgps::core::Country>& v, std::string code)
{
	auto is_target = [&code](const hgps::core::Country& obj) { return iequals(obj.code, code); };

	if (auto it = std::find_if(v.begin(), v.end(), is_target); it != v.end())
	{
		return (*it);
	}

	return std::nullopt;
}

