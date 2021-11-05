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
	std::string encoding;
	std::map<std::string,std::string> columns;
};

struct SettingsInfo
{
	std::string country{};
	float size_fraction{};
	std::string data_linkage{};
	std::vector<int> age_range;
};

struct SESInfo
{
	std::string function;
	std::vector<double> parameters;
};

struct BaselineInfo
{
	std::string format;
	std::string delimiter;
	std::string encoding;
	std::map<std::string, std::string> file_names;
};

struct RiskFactorInfo
{
	std::string name;
	short level;
	std::string proxy;
	std::vector<double> range;
};

struct ModellingInfo
{
	std::vector<RiskFactorInfo> risk_factors;
	std::string dynamic_risk_factor;
	std::unordered_map<std::string, std::string> models;
	BaselineInfo baseline_adjustment;
};

struct ResultInfo
{
	std::string folder{};
};

struct PolicyPeriodInfo
{
	int start_time{};
	std::optional<int> finish_time;

	std::string to_finish_time_str() const {
		if (finish_time.has_value()) {
			return std::to_string(finish_time.value());
		}

		return "null";
	}
};

struct PolicyScenarioInfo
{
	bool is_enabled {false};
	std::string impact_type;
	std::unordered_map<std::string, double> impacts;
	PolicyPeriodInfo active_period;
};

struct Configuration
{
	FileInfo file;
	SettingsInfo settings;
	SESInfo ses;
	ModellingInfo modelling;
	std::vector<std::string> diseases;
	std::optional<unsigned int> custom_seed;
	unsigned int start_time{};
	unsigned int stop_time{};
	unsigned int trial_runs{};
	unsigned int sync_timeout_ms{};
	PolicyScenarioInfo intervention;
	ResultInfo result;
};
