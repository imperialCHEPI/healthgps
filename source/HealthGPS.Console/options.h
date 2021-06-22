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

struct DataInfo
{
	std::string country{};

	std::string identity{};

	int start_value{};

	float delta_percent{};

	std::string data_linkage{};

	std::vector<int> age_range;
};

struct Configuration
{
	FileInfo file;

	DataInfo data_info;

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

// population
void to_json(json& j, const DataInfo& p) {
	j = json{ 
		{"country_code", p.country},
		{"identity", p.identity},
		{"start_value", p.start_value},
		{"delta_percent", p.delta_percent},
		{"data_linkage", p.data_linkage},
		{"age_range", p.age_range}};
}

void from_json(const json& j, DataInfo& p) {
	j.at("country_code").get_to(p.country);
	j.at("identity").get_to(p.identity);
	j.at("start_value").get_to(p.start_value);
	j.at("delta_percent").get_to(p.delta_percent);
	j.at("data_linkage").get_to(p.data_linkage);
	j.at("age_range").get_to(p.age_range);
}