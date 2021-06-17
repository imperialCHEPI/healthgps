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

#include "HealthGPS/api.h"
#include "options.h"

namespace fs = std::filesystem;

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

Configuration load_configuration(CommandOptions& options) {

	Configuration config;
	std::ifstream ifs(options.config_file, std::ifstream::in);
	if (ifs)
	{
		auto opt = json::parse(ifs);

		// input data file
		config.file = opt["inputs"]["file"].get<FileInfo>();
		fs::path full_path = config.file.name;
		if (full_path.is_relative()) {
			full_path = options.config_file.parent_path() / config.file.name;
			if (fs::exists(full_path)) {
				config.file.name = full_path.string();
				fmt::print("Input data file....: {}\n", config.file.name);
			}
		}

		if (!fs::exists(full_path)) {
			fmt::print(fg(fmt::color::red),
				"\n\nInput data file: {} not found.\n",
				full_path.string());
		}

		// Population and SES mapping
		config.population = opt["inputs"]["population"].get<Population>();
		opt["inputs"].at("ses_mapping").get_to(config.ses_mapping);

		// Run-time
		opt["running"]["start_time"].get_to(config.start_time);
		opt["running"]["stop_time"].get_to(config.stop_time);
		opt["running"]["trial_runs"].get_to(config.trial_runs);
		auto seed = opt["running"]["seed"].get<std::vector<unsigned int>>();
		if (seed.size() > 0) {
			config.custom_seed = seed[0];
		}
	}
	else
	{
		std::cout << std::format(
			"File {} doesn't exist.",
			options.config_file.string()) << std::endl;
	}

	ifs.close();
	return config;
}

hgps::Scenario create_scenario(Configuration& config)
{
	hgps::Scenario scenario(
		config.start_time,
		config.stop_time,
		config.trial_runs);

	scenario.country = config.population.country;
	scenario.custom_seed = config.custom_seed;

	return scenario;
}
