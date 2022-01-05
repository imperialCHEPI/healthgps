#include "configuration.h"
#include "jsonparser.h"

#include "HealthGPS/simple_policy_scenario.h"
#include "HealthGPS/marketing_scenario.h"

#include "HealthGPS.Core/scoped_timer.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <optional>
#include <fstream>
#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/chrono.h>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                     \
  hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

using namespace hgps;

std::string getTimeNowStr()
{
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
	MEASURE_FUNCTION();
	namespace fs = std::filesystem;

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

Configuration load_configuration(CommandOptions& options)
{
	MEASURE_FUNCTION();
	namespace fs = std::filesystem;

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

		fmt::print("Baseline factor adjustment:\n");
		for (auto& item : config.modelling.baseline_adjustment.file_names) {
			full_path = options.config_file.parent_path() / item.second;
			if (fs::exists(full_path)) {
				item.second = full_path.string();
				fmt::print("{:<14}, file: {}\n", item.first, full_path.string());
			}
			else {
				fmt::print(fg(fmt::color::red),
					"Adjustment type: {}, file: {} not found.\n", item.first, full_path.string());
			}
		}

		if (!config.modelling.dynamic_risk_factor.empty()) {
			auto found_dynamic_factor = false;
			for (auto& factor : config.modelling.risk_factors) {
				if (core::case_insensitive::equals(factor.name, config.modelling.dynamic_risk_factor)) {
					found_dynamic_factor = true;
					break;
				}
			}

			if (!found_dynamic_factor) {
				fmt::print(fg(fmt::color::red),
					"\nInvalid configuration, dynamic risk factor: {} is not a risk factor.\n",
					config.modelling.dynamic_risk_factor);
			}
		}

		// Run-time
		opt["running"]["start_time"].get_to(config.start_time);
		opt["running"]["stop_time"].get_to(config.stop_time);
		opt["running"]["trial_runs"].get_to(config.trial_runs);
		opt["running"]["sync_timeout_ms"].get_to(config.sync_timeout_ms);
		auto seed = opt["running"]["seed"].get<std::vector<unsigned int>>();
		if (seed.size() > 0) {
			config.custom_seed = seed[0];
		}

		auto cancers = std::vector<std::string>{};
		opt["running"]["diseases"].get_to(config.diseases);
		opt["running"]["cancers"].get_to(cancers);
		config.diseases.insert(config.diseases.end(), cancers.begin(), cancers.end());

		// Intervention Policy
		auto interventions = opt["running"]["interventions"];
		if (!interventions["active_type_id"].is_null() && !interventions["active_type_id"].empty()) {
			auto active_type = interventions["active_type_id"].get<std::string>();
			auto policy_types = interventions["types"];
			for (auto it = policy_types.begin(); it != policy_types.end(); ++it) {
				if (core::case_insensitive::equals(it.key(), active_type)) {
					config.intervention = it.value().get<PolicyScenarioInfo>();
					config.intervention.identifier = core::to_lower(it.key());
					config.has_active_intervention = true;
					break;
				}
			}
		}

		config.result = opt["results"].get<ResultInfo>();
	}
	else
	{
		std::cout << fmt::format(
			"File {} doesn't exist.",
			options.config_file.string()) << std::endl;
	}

	ifs.close();
	return config;
}

std::vector<core::DiseaseInfo> get_diseases(core::Datastore& data_api, Configuration& config)
{
	std::vector<core::DiseaseInfo> result;
	auto diseases = data_api.get_diseases();
	fmt::print("\nThere are {} diseases in storage, {} selected.\n",
		diseases.size(), config.diseases.size());

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

	auto run_info = RunInfo {
		.start_time = config.start_time,
		.stop_time = config.stop_time,
		.sync_timeout_ms = config.sync_timeout_ms,
		.seed = config.custom_seed
	};

	auto ses_mapping = SESDefinition {
		.fuction_name = config.ses.function,
		.parameters = config.ses.parameters
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

std::string create_output_file_name(const ResultInfo& info)
{
	namespace fs = std::filesystem;

	fs::path output_folder = info.folder;
	if (!fs::exists(output_folder)) {
		fs::create_directories(output_folder);
	}

	auto tp = std::chrono::system_clock::now();

	// filename token replacement
	auto file_name = info.file_name;
	std::size_t tk_end = 0;
	auto tk_start = file_name.find_first_of("{", tk_end);
	if (tk_start != std::string::npos) {
		auto timestamp_tk = std::string{ "{0:%F_%H-%M-}{1:%S}" };
		tk_end = file_name.find_first_of("}", tk_start + 1);
		if (tk_end != std::string::npos) {
			file_name.replace(tk_start, tk_end - tk_start + 1, timestamp_tk);
		}
	}

	auto log_file_name = fmt::format("HealthGPS_result_{0:%F_%H-%M-}{1:%S}.json", tp, tp.time_since_epoch());
	if (tk_end > 0) {
		log_file_name = fmt::format(file_name, tp, tp.time_since_epoch());
	}

	log_file_name = (output_folder / log_file_name).string();
	fmt::print(fg(fmt::color::yellow_green), "Results file: {}.\n", log_file_name);
	return log_file_name;
}

std::unique_ptr<hgps::InterventionScenario> create_intervention_scenario(
	SyncChannel& channel, const PolicyScenarioInfo& info)
{
	using namespace hgps;

	fmt::print(fg(fmt::color::light_coral), "\nIntervention policy: {}.\n", info.identifier);
	auto period = PolicyInterval(info.active_period.start_time, info.active_period.finish_time);
	auto risk_impacts = std::vector<PolicyImpact>{};
	for (auto& item : info.impacts) {
		risk_impacts.emplace_back(PolicyImpact{
			core::to_lower(item.risk_factor), item.impact_value, item.from_age, item.to_age });
	}

	if (info.identifier == "simple") {
		auto impact_type = PolicyImpactType::absolute;
		if (core::case_insensitive::equals(info.impact_type, "relative")) {
			impact_type = PolicyImpactType::relative;
		}
		else if (!core::case_insensitive::equals(info.impact_type, "absolute")) {
			throw std::logic_error(fmt::format("Unknown policy impact type: {}", info.impact_type));
		}

		auto definition = SimplePolicyDefinition(impact_type, risk_impacts, period);
		return std::make_unique<SimplePolicyScenario>(channel, std::move(definition));
	}

	if (info.identifier == "marketing") {
		auto definition = MarketingPolicyDefinition(period, risk_impacts);
		return std::make_unique<MarketingPolicyScenario>(channel, std::move(definition));
	}

	throw std::invalid_argument(
		fmt::format("Unknown intervention policy identifier: {}", info.identifier));
}
