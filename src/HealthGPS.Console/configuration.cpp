#include "configuration.h"
#include "jsonparser.h"
#include "version.h"

#include "HealthGPS/simple_policy_scenario.h"
#include "HealthGPS/marketing_scenario.h"
#include "HealthGPS/fiscal_scenario.h"
#include "HealthGPS/mtrandom.h"

#include "HealthGPS.Core/scoped_timer.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <fstream>
#include <fmt/color.h>
#include <fmt/chrono.h>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                     \
  hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

using namespace hgps;

std::string get_time_now_str()
{
	auto tp = std::chrono::system_clock::now();
	return fmt::format("{0:%F %H:%M:}{1:%S} {0:%Z}", tp, tp.time_since_epoch());
}

cxxopts::Options create_options()
{
	cxxopts::Options options("HealthGPS.Console", "Health-GPS microsimulation for policy options.");
	options.add_options()
		("f,file", "Configuration file full name.", cxxopts::value<std::string>())
		("s,storage", "Path to root folder of the data storage.", cxxopts::value<std::string>())
		("j,jobid", "The batch execution job identifier.", cxxopts::value<int>())
		("verbose", "Print more information about progress", cxxopts::value<bool>()->default_value("false"))
		("help", "Help about this application.")
		("version", "Print the application version number.");

	return options;
}

void print_app_title()
{
	fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
		"\n# Health-GPS Microsimulation for Policy Options #\n\n");

	fmt::print("Today: {}\n\n", get_time_now_str());
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
		cmd.verbose = false;
		auto result = options.parse(argc, argv);
		if (result.count("help"))
		{
			std::cout << options.help() << std::endl;
			cmd.success = false;
			return cmd;
		}

		if (result.count("version"))
		{
			fmt::print("Version {}\n\n", PROJECT_VERSION);
			cmd.success = false;
			return cmd;
		}

		cmd.verbose = result["verbose"].as<bool>();
		if (cmd.verbose) {
			fmt::print(fg(fmt::color::dark_salmon), "Verbose output enabled\n");
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
				"\nConfiguration file: {} not found.\n",
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
				"\nFile storage folder: {} not found.\n",
				cmd.storage_folder.string());
			cmd.exit_code = EXIT_FAILURE;
		}

		if (result.count("jobid")) {
			cmd.job_id = result["jobid"].as<int>();
			if (cmd.job_id < 1) {
				fmt::print(fg(fmt::color::red),
					"\nJob identifier value outside range: (0 < x) given: {}.\n",
					std::to_string(cmd.job_id));
				cmd.exit_code = EXIT_FAILURE;
			}
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
		if (!opt.contains("version")) {
			throw std::runtime_error("Invalid definition, file must have a schema version");
		}

		auto version = opt["version"].get<int>();
		if (version != 1) {
			throw std::runtime_error("definition schema version mismatch, supported = 1");
		}

		// application version
		config.app_name = PROJECT_NAME;
		config.app_version = PROJECT_VERSION;

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
		config.ses = opt["modelling"]["ses_model"].get<SESInfo>();

		// Modelling information
		config.modelling = opt["modelling"].get<ModellingInfo>();

		for (auto& model : config.modelling.risk_factor_models) {
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

		opt["running"]["diseases"].get_to(config.diseases);

		// Intervention Policy
		auto& interventions = opt["running"]["interventions"];
		if (!interventions["active_type_id"].is_null()) {
			auto active_type = interventions["active_type_id"].get<std::string>();
			auto& policy_types = interventions["types"];
			for (auto it = policy_types.begin(); it != policy_types.end(); ++it) {
				if (core::case_insensitive::equals(it.key(), active_type)) {
					config.intervention = it.value().get<PolicyScenarioInfo>();
					config.intervention.identifier = core::to_lower(it.key());
					config.has_active_intervention = true;
					break;
				}
			}
		}

		config.job_id = options.job_id;
		config.output = opt["output"].get<OutputInfo>();
		config.output.folder = expand_environment_variables(config.output.folder);
		if (!fs::exists(config.output.folder)) {
			fmt::print(fg(fmt::color::dark_salmon), "Creating output folder: {}\n", config.output.folder);
			if (!fs::create_directories(config.output.folder)) {
				throw std::runtime_error(fmt::format("Failed to create output folder: {}", config.output.folder));
			}
		}

		// verbosity
		config.verbosity = core::VerboseMode::none;
		if (options.verbose) {
			config.verbosity = core::VerboseMode::verbose;
		}
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

	auto comorbidities = config.output.comorbidities;
	auto diseases_number = static_cast<unsigned int>(diseases.size());
	if (comorbidities > diseases_number) {
		comorbidities = diseases_number;
		fmt::print(fg(fmt::color::salmon), "Comorbidities value: {}, set to # of diseases: {}.\n",
			config.output.comorbidities, comorbidities);
	}

	auto settings = Settings(country, config.settings.size_fraction, age_range);
	auto job_custom_seed = create_job_seed(config.job_id, config.custom_seed);
	auto run_info = RunInfo {
		.start_time = config.start_time,
		.stop_time = config.stop_time,
		.sync_timeout_ms = config.sync_timeout_ms,
		.seed = job_custom_seed,
		.verbosity = config.verbosity,
		.comorbidities = comorbidities,
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

std::string create_output_file_name(const OutputInfo& info, int job_id)
{
	namespace fs = std::filesystem;

	fs::path output_folder = expand_environment_variables(info.folder);
	auto tp = std::chrono::system_clock::now();
	auto timestamp_tk = fmt::format("{0:%F_%H-%M-}{1:%S}", tp, tp.time_since_epoch());

	// filename token replacement
	auto file_name = info.file_name;
	std::size_t tk_end = 0;
	auto tk_start = file_name.find_first_of("{", tk_end);
	if (tk_start != std::string::npos) {
		tk_end = file_name.find_first_of("}", tk_start + 1);
		if (tk_end != std::string::npos) {
			auto token_str = file_name.substr(tk_start, tk_end - tk_start + 1);
			if (!core::case_insensitive::equals(token_str,"{TIMESTAMP}")) {
				throw std::logic_error(fmt::format("Unknown output file token: {}", token_str));
			}

			file_name.replace(tk_start, tk_end - tk_start + 1, timestamp_tk);
		}
	}

	auto log_file_name = fmt::format("HealthGPS_result_{}.json", timestamp_tk);
	if (tk_end > 0) {
		log_file_name = file_name;
	}

	if (job_id > 0) {
		tk_start = log_file_name.find_last_of(".");
		if (tk_start != std::string::npos) {
			log_file_name.replace(tk_start, size_t{ 1 }, fmt::format("_{}.", std::to_string(job_id)));
		}
		else {
			log_file_name.append(fmt::format("_{}.json", std::to_string(job_id)));
		}
	}

	log_file_name = (output_folder / log_file_name).string();
	fmt::print(fg(fmt::color::yellow_green), "Output file: {}.\n", log_file_name);
	return log_file_name;
}

std::unique_ptr<hgps::InterventionScenario> create_intervention_scenario(
	SyncChannel& channel, const PolicyScenarioInfo& info)
{
	using namespace hgps;

	fmt::print(fg(fmt::color::light_coral), "\nIntervention policy: {}.\n\n", info.identifier);
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

	if (info.identifier == "fiscal") {
		auto impact_type = parse_fiscal_impact_type(info.impact_type);
		auto definition = FiscalPolicyDefinition(impact_type, period, risk_impacts);
		return std::make_unique<FiscalPolicyScenario>(channel, std::move(definition));
	}

	throw std::invalid_argument(
		fmt::format("Unknown intervention policy identifier: {}", info.identifier));
}

#ifdef _WIN32 
#pragma warning(disable : 4996)
#endif
std::string expand_environment_variables(const std::string& path)
{
	if (path.find("${") == std::string::npos) {
		return path;
	}

	std::string pre = path.substr(0, path.find("${"));
	std::string post = path.substr(path.find("${") + 2);
	if (post.find('}') == std::string::npos) {
		return path;
	}

	std::string variable = post.substr(0, post.find('}'));
	std::string value = "";

	post = post.substr(post.find('}') + 1);
	const char* v = std::getenv(variable.c_str()); // C4996, but safe here.
	if (v != NULL) value = std::string(v);

	return expand_environment_variables(pre + value + post);
}

std::optional<unsigned int> create_job_seed(int job_id, std::optional<unsigned int> user_seed)
{
	if (job_id > 0 && user_seed.has_value()) {
		auto rnd = hgps::MTRandom32{ user_seed.value() };
		auto jump_size = static_cast<unsigned long>(1.618 * job_id * std::pow(2, 16));
		rnd.discard(jump_size);
		return rnd();
	}

	return user_seed;
}
