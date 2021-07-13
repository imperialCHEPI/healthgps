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

struct FileInfo
{
	std::string name;

	std::string format;

	std::string delimiter;

	std::map<std::string,std::string> columns;
};

struct SettingsInfo
{
	std::string country{};

	/// @brief The data reference time, probably should be the simulation start time.
	int reference_time{};

	float size_fraction{};

	std::string data_linkage{};

	std::vector<int> age_range;
};

struct ModellingInfo
{
	std::unordered_map<std::string, int> risk_factors;

	std::string dynamic_risk_factor;

	std::unordered_map<std::string, std::string> models;
};

struct Configuration
{
	FileInfo file;

	SettingsInfo settings;

	std::map<std::string, std::string> ses_mapping;

	ModellingInfo modelling;

	std::optional<unsigned int> custom_seed;
	
	unsigned int start_time{};

	unsigned int stop_time{};

	unsigned int trial_runs{};
};
