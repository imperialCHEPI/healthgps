#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include "HealthGPS.Core\api.h"

using json = nlohmann::json;

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

struct Configuration
{
	FileInfo file;

	SettingsInfo settings;

	std::map<std::string, std::string> ses_mapping;

	std::optional<unsigned int> custom_seed;
	
	unsigned int start_time{};

	unsigned int stop_time{};

	unsigned int trial_runs{};
};

//--------------------------------------------------------
// JSON serialisation / de-serialisation
//--------------------------------------------------------

// data file
void to_json(json& j, const FileInfo& p) {
	j = json{
		{"name", p.name},
		{"format", p.format},
		{"delimiter", p.delimiter},
		{"columns", p.columns} };
}

void from_json(const json& j, FileInfo& p) {
	j.at("name").get_to(p.name);
	j.at("format").get_to(p.format);
	j.at("delimiter").get_to(p.delimiter);
	j.at("columns").get_to(p.columns);
}

// Data Information
void to_json(json& j, const SettingsInfo& p) {
	j = json{ 
		{"country_code", p.country},
		{"reference_time", p.reference_time},
		{"size_fraction", p.size_fraction},
		{"data_linkage", p.data_linkage},
		{"age_range", p.age_range}};
}

void from_json(const json& j, SettingsInfo& p) {
	j.at("country_code").get_to(p.country);
	j.at("reference_time").get_to(p.reference_time);
	j.at("size_fraction").get_to(p.size_fraction);
	j.at("data_linkage").get_to(p.data_linkage);
	j.at("age_range").get_to(p.age_range);
}