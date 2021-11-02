#pragma once

#include <chrono>
#include <ctime>
#include <iostream>
#include <optional>
#include <fstream>
#include <fmt/core.h>
#include <fmt/color.h>
#include <cxxopts.hpp>

#include "HealthGPS/api.h"

#include "csvparser.h"
#include "jsonparser.h"

using namespace hgps;
namespace fs = std::filesystem;

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
		config.ses = opt["inputs"]["ses"].get<SESInfo>();

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

		if (!config.modelling.dynamic_risk_factor.empty()) {
			// TODO: Search for dynamic factor
			// !config.modelling.risk_factors.contains(config.modelling.dynamic_risk_factor))
			fmt::print(fg(fmt::color::red),
				"\nInvalid configuration, dynamic risk factor: {} is not a risk factor.\n",
				config.modelling.dynamic_risk_factor);
		}

		// Run-time
		opt["running"]["start_time"].get_to(config.start_time);
		opt["running"]["stop_time"].get_to(config.stop_time);
		opt["running"]["trial_runs"].get_to(config.trial_runs);
		opt["running"]["sync_timeout_ms"].get_to(config.sync_timeout_ms);
		opt["running"]["loop_max_trials"].get_to(config.loop_max_trials);
		auto seed = opt["running"]["seed"].get<std::vector<unsigned int>>();
		if (seed.size() > 0) {
			config.custom_seed = seed[0];
		}

		auto cancers = std::vector<std::string>{};
		opt["running"]["diseases"].get_to(config.diseases);
		opt["running"]["cancers"].get_to(cancers);
		config.diseases.insert(config.diseases.end(), cancers.begin(), cancers.end());
		if (!opt["running"]["intervention"].empty()) {
			config.intervention = opt["running"]["intervention"].get<PolicyScenarioInfo>();
		}

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

HierarchicalLinearModelDefinition load_static_risk_model_definition(
	std::string model_filename, hgps::BaselineAdjustment& baseline_data) {
	MEASURE_FUNCTION();

	std::map<int, HierarchicalLevel> levels;
	std::unordered_map<std::string, LinearModel> models;
	std::ifstream ifs(model_filename, std::ifstream::in);
	if (ifs) {
		try {
			auto opt = json::parse(ifs);
			HierarchicalModelInfo model_info;
			model_info.models = opt["models"].get<std::unordered_map<std::string, LinearModelInfo>>();
			model_info.levels = opt["levels"].get<std::unordered_map<std::string, HierarchicalLevelInfo>>();

			for (auto& model_item : model_info.models) {
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
					.residuals_standard_deviation = at.residuals_standard_deviation,
					.rsquared = at.rsquared
					});
			}

			for (auto& level_item : model_info.levels) {
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
		}
		catch (const std::exception& ex) {
			fmt::print(fg(fmt::color::red),
				"Failed to parse model: {:<7}, file: {}. {}\n",
				"static", model_filename, ex.what());
		}
	}
	else {
		fmt::print(fg(fmt::color::red),
			"Model: {:<7}, file: {} not found.\n", "static", model_filename);
	}

	ifs.close();
	return HierarchicalLinearModelDefinition{ std::move(models), std::move(levels), baseline_data };
}

LiteHierarchicalModelDefinition load_dynamic_risk_model_info(std::string model_filename) {
	MEASURE_FUNCTION();

	std::map<std::string, std::string> variables;
	std::map<core::IntegerInterval, AgeGroupGenderEquation> equations;

	std::ifstream ifs(model_filename, std::ifstream::in);
	if (ifs) {
		try {
			auto opt = json::parse(ifs);
			auto info = LiteHierarchicalModelInfo{};
			info.variables = opt["Variables"].get<std::vector<VariableInfo>>();
			for (auto it : opt["Equations"].items()) {
				auto age_key = it.key();
				info.equations.emplace(age_key, std::map<std::string, std::vector<FactorDynamicEquationInfo>>());

				for (auto sit : it.value().items()) {
					auto gender_key = sit.key();
					auto gender_funcs = sit.value().get<std::vector<FactorDynamicEquationInfo>>();
					info.equations.at(age_key).emplace(gender_key, gender_funcs);
				}
			}

			for (auto& item : info.variables) {
				variables.emplace(core::to_lower(item.name), core::to_lower(item.factor));
			}

			for (auto& age_grp : info.equations) {
				auto limits = core::split_string(age_grp.first, "-");
				auto age_key = core::IntegerInterval(std::stoi(limits[0].data()), std::stoi(limits[1].data()));
				auto age_equations = AgeGroupGenderEquation{ .age_group = age_key };
				for (auto& gender : age_grp.second)	{
					
					if (core::case_insensitive::equals("male", gender.first)) {
						for (auto& func : gender.second) {
							auto function = FactorDynamicEquation{ .name = func.name };
							function.residuals_standard_deviation = func.residuals_standard_deviation;
							for (auto& coeff : func.coefficients) {
								function.coefficients.emplace(core::to_lower(coeff.first), coeff.second);
							}

							age_equations.male.emplace(core::to_lower(func.name), function);
						}
					}
					else if (core::case_insensitive::equals("female", gender.first)) {
						for (auto& func : gender.second) {
							auto function = FactorDynamicEquation{ .name = func.name };
							function.residuals_standard_deviation = func.residuals_standard_deviation;
							for (auto& coeff : func.coefficients) {
								function.coefficients.emplace(core::to_lower(coeff.first), coeff.second);
							}

							age_equations.female.emplace(core::to_lower(func.name), function);
						}
					}
					else {
						fmt::print(fg(fmt::color::red),
							"Unknown model gender type: {}.\n", gender.first);
					}
				}

				equations.emplace(age_key, std::move(age_equations));
			}
		}
		catch (const std::exception& ex) {
			fmt::print(fg(fmt::color::red),
				"Failed to parse model: {:<7}, file: {}. {}\n",
				"static", model_filename, ex.what());
		}
	}
	else {
		fmt::print(fg(fmt::color::red),
			"Model: {:<7}, file: {} not found.\n", "static", model_filename);
	}

	ifs.close();
	return LiteHierarchicalModelDefinition{ std::move(equations), std::move(variables) };
}

hgps::BaselineAdjustment load_baseline_adjustments(
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

void register_risk_factor_model_definitions(const ModellingInfo info,
	hgps::CachedRepository& repository, hgps::BaselineAdjustment& baseline_data) {
	MEASURE_FUNCTION();
	for (auto& model : info.models) {
		HierarchicalModelType model_type;
		if (core::case_insensitive::equals(model.first, "static")) {
			model_type = HierarchicalModelType::Static;
			auto model_definition = load_static_risk_model_definition(model.second, baseline_data);
			repository.register_linear_model_definition(model_type, std::move(model_definition));
			
		}
		else if (core::case_insensitive::equals(model.first, "dynamic")) {
			model_type = HierarchicalModelType::Dynamic;
			auto model_definition = load_dynamic_risk_model_info(model.second);
			repository.register_lite_linear_model_definition(model_type, std::move(model_definition));
		}
		else {
			fmt::print(fg(fmt::color::red),
				"Unknown hierarchical model type: {}.\n", model.first);
			continue;
		}
	}
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
		.sync_timeout_ms = config.sync_timeout_ms,
		.loop_max_trials = config.loop_max_trials,
		.seed = config.custom_seed
	};

	auto ses_mapping = SESMapping
	{
		.update_interval = config.ses.update_interval,
		.update_max_age = config.ses.update_max_age,
		.entries = config.ses.mapping
	};

	auto mapping = std::vector<MappingEntry>();
	for (auto& item : config.modelling.risk_factors) {
		if (core::case_insensitive::equals(item.name, config.modelling.dynamic_risk_factor)) {
			if (item.range.empty()) {
				mapping.emplace_back(MappingEntry(item.name, item.level, item.proxy, true));
			}
			else {
				auto boundary = FactorRange{ item.range[0], item.range[1] };
				mapping.emplace_back(MappingEntry(item.name, item.level, item.proxy, boundary, true));
			}
		}
		else if (!item.range.empty()) {
			auto boundary = FactorRange{ item.range[0], item.range[1] };
			mapping.emplace_back(MappingEntry{ item.name, item.level, item.proxy, boundary });
		}
		else {
			mapping.emplace_back(MappingEntry{ item.name, item.level, item.proxy });
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

hgps::InterventionScenario create_intervention_scenario(SyncChannel& channel, const PolicyScenarioInfo& info) {
	using namespace hgps;
	auto impact_type = PolicyImpactType::absolute;
	if (core::case_insensitive::equals(info.impact_type, "relative")) {
		impact_type = PolicyImpactType::relative;
	}
	else if (!core::case_insensitive::equals(info.impact_type, "absolute")) {
		throw std::logic_error(std::format("Unknown policy impact type: {}", info.impact_type));
	}

	auto risk_impacts = std::vector<PolicyImpact>{};
	for (auto& item : info.impacts) {
		risk_impacts.emplace_back(PolicyImpact{ core::to_lower(item.first), item.second });
	}

	auto period = PolicyInterval(info.active_period.start_time, info.active_period.finish_time);
	auto definition = PolicyDefinition(impact_type, risk_impacts, period);
	return InterventionScenario{ channel, std::move(definition) };
}