#pragma once
#include <cxxopts.hpp>

#include "HealthGPS/modelinput.h"
#include "HealthGPS/scenario.h"
#include "HealthGPS/intervention_scenario.h"
#include "HealthGPS/healthgps.h"

#include "HealthGPS.Core/api.h"

#include "result_file_writer.h"
#include "options.h"

namespace host
{
	/// @brief Get a string representation of current system time
	/// @return The system time as string
	std::string get_time_now_str();

	/// @brief Creates the command-line interface (CLI) options
	/// @return Health-GPS CLI options
	cxxopts::Options create_options();

	/// @brief Prints application start-up messages
	void print_app_title();

	/// @brief Parses the command-line interface (CLI) arguments
	/// @param options The valid CLI options
	/// @param argc Number of input arguments
	/// @param argv List of input arguments
	/// @return User command-line options
	CommandOptions parse_arguments(cxxopts::Options& options, int& argc, char* argv[]);

	/// @brief Loads the input configuration file, *.json, information
	/// @param options User command-line options 
	/// @return The configuration file information
	Configuration load_configuration(CommandOptions& options);

	/// @brief Creates the configuration output folder for result files
	/// @param folder_path Full path to output folder
	/// @param num_retries Number of attempts before giving up
	/// @return true for successful creation, otherwise false
	bool create_output_folder(std::filesystem::path folder_path, unsigned int num_retries = 3);

	/// @brief Gets the collection of diseases that matches the selected input list
	/// @param data_api The back-end data store instance to be used.
	/// @param config User configuration file instance
	/// @return Collection of matching diseases information
	std::vector<hgps::core::DiseaseInfo> get_diseases_info(hgps::core::Datastore& data_api, Configuration& config);

	/// @brief Creates Health-GPS inputs instance from configuration
	/// @param input_table The input dataset instance
	/// @param country Target country information
	/// @param config User input configuration instance
	/// @param diseases Selected diseases for experiment
	/// @return The respective model input instance
	hgps::ModelInput create_model_input(hgps::core::DataTable& input_table, hgps::core::Country country,
		Configuration& config, std::vector<hgps::core::DiseaseInfo> diseases);

	/// @brief Creates the full output file name from user input configuration
	/// @param info User output information, may contain relative path and environment variables
	/// @param job_id Simulation job identifier
	/// @return Output file full name
	std::string create_output_file_name(const poco::OutputInfo& info, int job_id);

	/// @brief Creates the simulation results file logger instance
	/// @param config User input configuration information
	/// @param input Model input instance
	/// @return The respective file writer instance 
	ResultFileWriter create_results_file_logger(const Configuration& config, const hgps::ModelInput& input);

	/// @brief Creates the baseline scenario instance
	/// @param channel Synchronization channel instance for data exchange
	/// @return The baseline scenario instance
	std::unique_ptr<hgps::Scenario> create_baseline_scenario(hgps::SyncChannel& channel);

	/// @brief Creates the baseline simulation engine instance
	/// @param channel Synchronization channel for data exchange instance
	/// @param factory Simulation module factory instance
	/// @param event_bus Shared event bus instance for streaming messages
	/// @param input Model inputs instance
	/// @return The respective simulation engine instance
	hgps::HealthGPS create_baseline_simulation(hgps::SyncChannel& channel, hgps::SimulationModuleFactory& factory,
		hgps::EventAggregator& event_bus, hgps::ModelInput& input);

	/// @brief Creates the intervention scenario instance
	/// @param channel Synchronization channel instance for data exchange
	/// @param info Intervention configuration information
	/// @return The respective intervention scenario instance
	/// @throws std::invalid_argument Thrown if intervention `identifier` is unknown.
	std::unique_ptr<hgps::InterventionScenario> create_intervention_scenario(
		hgps::SyncChannel& channel, const poco::PolicyScenarioInfo& info);

	/// @brief Creates the intervention simulation engine instance
	/// @param channel Synchronization channel for data exchange instance
	/// @param factory Simulation module factory instance
	/// @param event_bus Shared event bus instance for streaming messages
	/// @param input Model inputs instance
	/// @param info Intervention policy definition
	/// @return The respective simulation engine instance
	hgps::HealthGPS create_intervention_simulation(hgps::SyncChannel& channel, hgps::SimulationModuleFactory& factory,
		hgps::EventAggregator& event_bus, hgps::ModelInput& input, const poco::PolicyScenarioInfo& info);

	/// @brief Expand environment variables in path to respective values
	/// @param path The source path to information
	/// @return The resulting full path
	std::string expand_environment_variables(const std::string& path);

	/// @brief Creates the experiment random number generator seed
	/// @param job_id Optional batch job identifier
	/// @param user_seed User input custom seed for experiment
	/// @return The experiment seed, if user provide a seed
	std::optional<unsigned int> create_job_seed(int job_id, std::optional<unsigned int> user_seed);
}