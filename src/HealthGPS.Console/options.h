#pragma once
#include "HealthGPS.Core/forward_type.h"

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <filesystem>
#include <optional>

struct CommandOptions
{
	bool success{};
	int exit_code{};
	std::filesystem::path config_file{};
	std::filesystem::path storage_folder{};
	bool verbose{};
	int job_id{};
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
	short level{};
	std::string proxy;
	std::vector<double> range;
};

struct ModellingInfo
{
	std::vector<RiskFactorInfo> risk_factors;
	std::string dynamic_risk_factor;
	std::unordered_map<std::string, std::string> risk_factor_models;
	BaselineInfo baseline_adjustment;
};

struct OutputInfo
{
	unsigned int comorbidities{};
	std::string folder{};
	std::string file_name{};
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

struct PolicyImpactInfo
{
	std::string risk_factor{};
	double impact_value{};
	unsigned int from_age{};
	std::optional<unsigned int> to_age{};
	std::string to_age_str() const {
		if (to_age.has_value()) {
			return std::to_string(to_age.value());
		}

		return "null";
	}
};

struct PolicyAdjustmentInfo
{
	std::string risk_factor{};
	double value{};
};

struct PolicyScenarioInfo
{
	std::string identifier{};
	PolicyPeriodInfo active_period;
	std::vector<double> dynamics;
	std::vector<double> coefficients;
	std::vector<double> coverage_rates;
	std::optional<unsigned int> coverage_cutoff_time{};
	std::optional<unsigned int> child_cutoff_age{};
	std::vector<PolicyAdjustmentInfo> adjustments;
	std::string impact_type{};
	std::vector<PolicyImpactInfo> impacts;

	std::string to_coverage_cutoff_time_str() const {
		if (coverage_cutoff_time.has_value()) {
			return std::to_string(coverage_cutoff_time.value());
		}

		return "null";
	}

	std::string to_child_cutoff_age_str() const {
		if (child_cutoff_age.has_value()) {
			return std::to_string(child_cutoff_age.value());
		}

		return "null";
	}
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
	bool has_active_intervention{false};
	PolicyScenarioInfo intervention;
	OutputInfo output;
	hgps::core::VerboseMode verbosity{};
	int job_id{};
	std::string app_name;
	std::string app_version;
};
