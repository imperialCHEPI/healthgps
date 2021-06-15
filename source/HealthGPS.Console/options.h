#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>

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

	std::unordered_map<std::string,std::string> columns;
};

struct Population
{
	std::string country{};

	std::string identity{};

	int start_value{};

	double delta_percent{};
};

struct Configuration
{
	FileInfo file;

	Population population;

	std::unordered_map<std::string, std::string> ses_mapping;

	std::optional<unsigned int> custom_seed;
	
	int start_time{};

	int stop_time{};

	int trial_runs{};
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

// population
void to_json(json& j, const Population& p) {
	j = json{ 
		{"country", p.country},
		{"identity", p.identity},
		{"start_value", p.start_value},
		{"delta_percent", p.delta_percent}};
}

void from_json(const json& j, Population& p) {
	j.at("country").get_to(p.country);
	j.at("identity").get_to(p.identity);
	j.at("start_value").get_to(p.start_value);
	j.at("delta_percent").get_to(p.delta_percent);
}