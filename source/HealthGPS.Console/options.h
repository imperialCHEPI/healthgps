#pragma once
#include <filesystem>

struct CommandOptions
{
	bool success{};

	int exit_code{};

	std::filesystem::path config_file{};

	std::filesystem::path storage_folder{};

	bool verbose{};
};

struct Configuration
{
	std::string country;

	std::optional<unsigned int> custom_seed;
	
	int start_time;

	int stop_time;

	int trial_runs;
};
