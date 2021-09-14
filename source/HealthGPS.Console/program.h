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
#include "HealthGPS.Core/scoped_timer.h"

#include "csvparser.h"
#include "jsonparser.h"

using namespace hgps;
namespace fs = std::filesystem;

#define USE_TIMER 1

#if USE_TIMER
#define MEASURE_FUNCTION()                                                     \
  core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

std::string getTimeNowStr() {
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::string s(30, '\0');

	struct tm localtime;

	localtime_s(&localtime, &now);

	std::strftime(&s[0], s.size(), "%c", &localtime);

	return s;
}

cxxopts::Options create_options() {
	cxxopts::Options options("HealthGPS.Console", "Health-GPS microsimulation for policy options.");
	options.add_options()
		("h,help", "Help about this application.")
		("f,file", "Configuration file full name.", cxxopts::value<std::string>())
		("s,storage", "Path to root folder of the data storage.", cxxopts::value<std::string>())
		("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"));

	return options;
}

CommandOptions parse_arguments(cxxopts::Options& options, int& argc, char* argv[]) {
	MEASURE_FUNCTION();
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
				fmt::print("Configuration file..: {}\n", cmd.config_file.string());
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
				fmt::print("File storage folder.: {}\n", cmd.storage_folder.string());
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
	MEASURE_FUNCTION();
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
				fmt::print("Input data file.....: {}\n", config.file.name);
			}
		}

		if (!fs::exists(full_path)) {
			fmt::print(fg(fmt::color::red),
				"\nInput data file: {} not found.\n",
				full_path.string());
		}

		// Settings and SES mapping
		config.settings = opt["inputs"]["settings"].get<SettingsInfo>();
		opt["inputs"].at("ses_mapping").get_to(config.ses_mapping);

		// Modelling information
		config.modelling = opt["modelling"].get<ModellingInfo>();
		for (auto& model : config.modelling.models) {
			full_path = model.second;
			if (full_path.is_relative()) {
				full_path = options.config_file.parent_path() / model.second;
				if (fs::exists(full_path)) {
					model.second = full_path.string();
					fmt::print("Model: {:<7}, file: {}\n", model.first, model.second);
				}
			}

			if (!fs::exists(full_path)) {
				fmt::print(fg(fmt::color::red),
					"Model: {:<7}, file: {} not found.\n", model.first, full_path.string());
			}
		}

		full_path = options.config_file.parent_path() / config.modelling.baseline_adjustment.file_name;
		if (fs::exists(full_path)) {
			config.modelling.baseline_adjustment.file_name = full_path.string();
		}

		if (!config.modelling.dynamic_risk_factor.empty() &&
			!config.modelling.risk_factors.contains(config.modelling.dynamic_risk_factor)) {
			fmt::print(fg(fmt::color::red),
				"\nInvalid configuration, dynamic risk factor: {} is not a risk factor.\n",
				config.modelling.dynamic_risk_factor);
		}

		// Run-time
		opt["running"]["start_time"].get_to(config.start_time);
		opt["running"]["stop_time"].get_to(config.stop_time);
		opt["running"]["trial_runs"].get_to(config.trial_runs);
		auto seed = opt["running"]["seed"].get<std::vector<unsigned int>>();
		if (seed.size() > 0) {
			config.custom_seed = seed[0];
		}

		opt["running"]["diseases"].get_to(config.diseases);
		config.result = opt["results"].get<ResultInfo>();
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

std::unordered_map<std::string, HierarchicalModelInfo> load_risk_model_info(ModellingInfo info) {
	MEASURE_FUNCTION();
	std::unordered_map<std::string, HierarchicalModelInfo> modelsInfo;
	for (auto& model : info.models) {
		std::ifstream ifs(model.second, std::ifstream::in);
		if (ifs) {
			try {
				auto opt = json::parse(ifs);
				HierarchicalModelInfo hmodel;
				hmodel.models = opt["models"].get<std::unordered_map<std::string, LinearModelInfo>>();
				hmodel.levels = opt["levels"].get<std::unordered_map<std::string, HierarchicalLevelInfo>>();
				modelsInfo.emplace(model.first, hmodel);
			}
			catch (const std::exception& ex) {
				fmt::print(fg(fmt::color::red),
					"Failed to parse model: {:<7}, file: {}. {}\n",
					model.first, model.second, ex.what());
			}
		}
		else {
			fmt::print(fg(fmt::color::red),
				"Model: {:<7}, file: {} not found.\n",
				model.first, model.second);
		}

		ifs.close();
	}

	fmt::print(" Risk Factors: {}, models: {}\n", info.risk_factors.size(), modelsInfo.size());
	fmt::print("|{0:-^{1}}|\n", "", 43);
	fmt::print("| {:<8} : {:>8} : {:>8} : {:>8} |\n", "Type", "Models", "Levels", "Factors");
	fmt::print("|{0:-^{1}}|\n", "", 43);
	for (auto& model : modelsInfo) {
		auto max_model = std::max_element(model.second.models.begin(), model.second.models.end(),
			[](const auto& lhs, const auto& rhs) {
				return lhs.second.coefficients.size() < rhs.second.coefficients.size();
			});

		fmt::print("| {:<8} : {:>8} : {:>8} : {:>8} |\n",
			model.first, model.second.models.size(), model.second.levels.size(),
			max_model->second.coefficients.size());
	}

	fmt::print("|{0:_^{1}}|\n\n", "", 43);
	return modelsInfo;
}

hgps::BaselineAdjustment load_baseline_adjustment(
	const BaselineInfo& info, const unsigned int& start_time, const unsigned int& stop_time) {
	MEASURE_FUNCTION();
	if (!info.is_enabled) {
		return BaselineAdjustment{};
	}

	try {
		auto time_range = hc::IntegerInterval{static_cast<int>(start_time), static_cast<int>(stop_time) };
		if (core::case_insensitive::equals(info.format, "CSV")) {
			auto data = load_baseline_csv(time_range, info.file_name, info.delimiter);
			return BaselineAdjustment{ std::move(data) };
		}
		else if (core::case_insensitive::equals(info.format, "JSON")) {
			throw std::logic_error("JSON support not yet implemented.");

			std::ifstream ifs(info.file_name, std::ifstream::in);
			if (ifs) {
				// TODO: JSON Parser
			}
			else {
				fmt::print(fg(fmt::color::red),
					"Baseline adjustment file: {} not found.\n", info.file_name);
			}

			ifs.close();
		}
		else {
			throw std::logic_error("Unsupported file format: " + info.format);
		}
	}
	catch (const std::exception& ex) {
		fmt::print(fg(fmt::color::red),
			"Failed to parse model file: {}. {}\n", info.file_name, ex.what());
		throw;
	}
}

std::shared_ptr<RiskFactorModule> build_risk_factor_module(
	const ModellingInfo info, hgps::BaselineAdjustment& baseline_data) {
	MEASURE_FUNCTION();
	auto config_models_info = load_risk_model_info(info);

	// Create hierarchical models
	std::unordered_map<HierarchicalModelType,
		std::shared_ptr<HierarchicalLinearModel>> linear_models;
	for (auto& config_model_type : config_models_info) {
		HierarchicalModelType model_type;
		if (core::case_insensitive::equals(config_model_type.first, "static")) {
			model_type = HierarchicalModelType::Static;
		}
		else if (core::case_insensitive::equals(config_model_type.first, "dynamic")) {
			model_type = HierarchicalModelType::Dynamic;
		}
		else {
			fmt::print(fg(fmt::color::red),
				"Unknown hierarchical model type: {}.\n", config_model_type.first);
			continue;
		}

		// TODO: independent work, can be done in parallel
		std::unordered_map<std::string, LinearModel> models;
		for (auto& model_item : config_model_type.second.models) {
			auto& at = model_item.second;

			std::unordered_map<std::string, Coefficient> coeffs;
			for (auto& pair : at.coefficients) {
				coeffs.emplace(core::to_lower(pair.first), Coefficient{
						.value = pair.second.value,
						.pvalue = pair.second.pvalue,
						.tvalue = pair.second.tvalue,
						.std_error = pair.second.std_error
					});
			}

			models.emplace(core::to_lower(model_item.first), LinearModel{
				.coefficients = coeffs,
				.fitted_values = at.fitted_values,
				.residuals = at.fitted_values,
				.rsquared = at.rsquared
				});
		}

		std::map<int, HierarchicalLevel> levels;
		for (auto& level_item : config_model_type.second.levels) {
			auto& at = level_item.second;
			std::unordered_map<std::string, int> col_names;
			for (int i = 0; i < at.variables.size(); i++) {
				col_names.emplace(core::to_lower(at.variables[i]), i);
			}

			levels.emplace(std::stoi(level_item.first), HierarchicalLevel{
				.variables = col_names,
				.transition = core::DoubleArray2D(
					at.transition.rows, at.transition.cols, at.transition.data),

				.inverse_transition = core::DoubleArray2D(at.inverse_transition.rows,
					at.inverse_transition.cols, at.inverse_transition.data),

				.residual_distribution = core::DoubleArray2D(at.residual_distribution.rows,
					at.residual_distribution.cols, at.residual_distribution.data),

				.correlation = core::DoubleArray2D(at.correlation.rows,
					at.correlation.cols, at.correlation.data),

				.variances = at.variances
				});
		}

		if (model_type == HierarchicalModelType::Static) {
			linear_models.emplace(model_type,
				std::make_shared<StaticHierarchicalLinearModel>(std::move(models), std::move(levels), baseline_data));
		}
		else if (model_type == HierarchicalModelType::Dynamic) {
			linear_models.emplace(model_type,
				std::make_shared<DynamicHierarchicalLinearModel>(std::move(models), std::move(levels), baseline_data));
		}
	}

	return std::make_shared<RiskFactorModule>(std::move(linear_models));
}

std::vector<core::DiseaseInfo> get_diseases(core::Datastore& data_api, Configuration& config) {
	std::vector<core::DiseaseInfo> result;
	auto diseases = data_api.get_diseases();
	fmt::print("\nThere are {} diseases in storage.\n", diseases.size());
	for (auto& code : config.diseases) {
		auto item = data_api.get_disease_info(core::to_lower(code));
		if (item.has_value()) {
			result.emplace_back(item.value());
		}
		else {
			fmt::print(fg(fmt::color::salmon), "Unknown disease code: {}.\n", code);
		}
	}

	return result;
}

hgps::Scenario create_scenario(Configuration& config)
{
	hgps::Scenario scenario(
		config.start_time,
		config.stop_time,
		config.trial_runs);

	scenario.country = config.settings.country;
	scenario.custom_seed = config.custom_seed;

	return scenario;
}

auto find_by_value(SESMapping ses, std::string value) {
	for (auto& i : ses.entries) {
		if (core::case_insensitive::equals(i.second, value)) {
			return i.first;
		}
	}

	return std::string{};
}

ModelInput create_model_input(core::DataTable& input_table, core::Country country,
	Configuration& config, std::vector<core::DiseaseInfo> diseases)
{
	// Create simulation configuration
	auto age_range = core::IntegerInterval(
		config.settings.age_range.front(), config.settings.age_range.back());

	auto settings = Settings(country, config.settings.size_fraction,
		config.settings.data_linkage, age_range);

	auto run_info = RunInfo{
		.start_time = config.start_time,
		.stop_time = config.stop_time,
		.seed = config.custom_seed
	};

	auto ses_mapping = SESMapping();
	ses_mapping.entries.insert(config.ses_mapping.begin(), config.ses_mapping.end());

	auto mapping = std::vector<MappingEntry>();
	for (auto& item : config.modelling.risk_factors) {
		if (core::case_insensitive::equals(item.first, config.modelling.dynamic_risk_factor)) {
			mapping.emplace_back(MappingEntry(item.first, item.second,
				find_by_value(ses_mapping, item.first), true));
		}
		else {
			mapping.emplace_back(MappingEntry(item.first, item.second,
				find_by_value(ses_mapping, item.first)));
		}
	}

	return ModelInput(input_table, settings, run_info, ses_mapping,
		HierarchicalMapping(std::move(mapping)), diseases);
}

std::string create_output_file_name(const ResultInfo& info) {
	fs::path output_folder = info.folder;
	if (!fs::exists(output_folder)) {
		fs::create_directories(output_folder);
	}

	auto tp = std::chrono::system_clock::now();
	auto local_tp = std::chrono::zoned_time{ std::chrono::current_zone(), tp };

	auto log_file_name = std::format("HealthGPS_result_{:%F_%H-%M-%S}.json", local_tp);
	log_file_name = (output_folder / log_file_name).string();
	fmt::print(fg(fmt::color::yellow_green), "Results file: {}.\n", log_file_name);
	return log_file_name;
}