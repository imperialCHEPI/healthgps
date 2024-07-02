/**
 * @file
 * @brief Main header file for functionality related to loading config files
 *
 * This file contains definitions for the main functions required to load JSON-formatted
 * configuration files from disk.
 */
#pragma once

#include "data_source.h"
#include "poco.h"
#include "version.h"

#include "HealthGPS.Core/api.h"
#include "HealthGPS/intervention_scenario.h"
#include "HealthGPS/modelinput.h"
#include "HealthGPS/scenario.h"
#include "HealthGPS/simulation.h"

#include <optional>
#include <stdexcept>

namespace hgps::input {

/// @brief Defines the application configuration data structure
struct Configuration {
    /// @brief The root path for configuration files
    std::filesystem::path root_path;

    /// @brief Static data source for the simulation (either URL or path)
    std::optional<DataSource> data_source;

    /// @brief The input data file details
    FileInfo file;

    /// @brief Experiment population settings
    SettingsInfo settings;

    /// @brief Socio-economic status (SES) model inputs
    SESInfo ses;

    /// @brief User defined model and parameters information
    ModellingInfo modelling;

    /// @brief List of diseases to include in experiment
    std::vector<std::string> diseases;

    /// @brief Simulation initialisation custom seed value, optional
    std::optional<unsigned int> custom_seed;

    /// @brief The experiment start time (simulation clock)
    unsigned int start_time{};

    /// @brief The experiment stop time (simulation clock)
    unsigned int stop_time{};

    /// @brief The number of simulation runs (replications) to execute
    unsigned int trial_runs{};

    /// @brief Baseline to intervention data synchronisation time out (milliseconds)
    unsigned int sync_timeout_ms{};

    /// @brief The active intervention policy definition
    std::optional<PolicyScenarioInfo> active_intervention;

    /// @brief Experiment output folder and file information
    OutputInfo output;

    /// @brief Application logging verbosity mode
    hgps::core::VerboseMode verbosity{};

    /// @brief Experiment batch job identifier
    int job_id{};

    /// @brief Experiment model name
    const char *app_name = PROJECT_NAME;

    /// @brief Experiment model version
    const char *app_version = PROJECT_VERSION;
};

/// @brief Represents an error that occurred with the format of a config file
class ConfigurationError : public std::runtime_error {
  public:
    ConfigurationError(const std::string &msg);
};

/// @brief Loads the input configuration file, *.json, information
/// @param config_file Path to config file
/// @param job_id Job ID (for HPC)
/// @param verbose Set log verbosity for simulation
/// @return The configuration file information
Configuration get_configuration(const std::filesystem::path &config_file, int job_id, bool verbose);

/// @brief Gets the collection of diseases that matches the selected input list
/// @param data_api The back-end data store instance to be used.
/// @param config User configuration file instance
/// @return Collection of matching diseases information
std::vector<hgps::core::DiseaseInfo> get_diseases_info(hgps::core::Datastore &data_api,
                                                       Configuration &config);

/// @brief Creates Health-GPS inputs instance from configuration
/// @param input_table The input dataset instance
/// @param country Target country information
/// @param config User input configuration instance
/// @param diseases Selected diseases for experiment
/// @return The respective model input instance
hgps::ModelInput create_model_input(hgps::core::DataTable &input_table, hgps::core::Country country,
                                    const Configuration &config,
                                    std::vector<hgps::core::DiseaseInfo> diseases);

/// @brief Creates the full output file name from user input configuration
/// @param info User output information, may contain relative path and environment variables
/// @param job_id Simulation job identifier
/// @return Output file full name
std::string create_output_file_name(const OutputInfo &info, int job_id);

/// @brief Creates the baseline scenario instance
/// @param channel Synchronization channel instance for data exchange
/// @return The baseline scenario instance
std::unique_ptr<hgps::Scenario> create_baseline_scenario(hgps::SyncChannel &channel);

/// @brief Creates the baseline simulation engine instance
/// @param channel Synchronization channel for data exchange instance
/// @param factory Simulation module factory instance
/// @param event_bus Shared event bus instance for streaming messages
/// @param input Model inputs instance
/// @return The respective simulation engine instance
hgps::Simulation create_baseline_simulation(hgps::SyncChannel &channel,
                                            hgps::SimulationModuleFactory &factory,
                                            hgps::EventAggregator &event_bus,
                                            hgps::ModelInput &input);

/// @brief Creates the intervention scenario instance
/// @param channel Synchronization channel instance for data exchange
/// @param info Intervention configuration information
/// @return The respective intervention scenario instance
/// @throws std::invalid_argument Thrown if intervention `identifier` is unknown.
std::unique_ptr<hgps::InterventionScenario>
create_intervention_scenario(hgps::SyncChannel &channel, const PolicyScenarioInfo &info);

/// @brief Creates the intervention simulation engine instance
/// @param channel Synchronization channel for data exchange instance
/// @param factory Simulation module factory instance
/// @param event_bus Shared event bus instance for streaming messages
/// @param input Model inputs instance
/// @param info Intervention policy definition
/// @return The respective simulation engine instance
hgps::Simulation create_intervention_simulation(hgps::SyncChannel &channel,
                                                hgps::SimulationModuleFactory &factory,
                                                hgps::EventAggregator &event_bus,
                                                hgps::ModelInput &input,
                                                const PolicyScenarioInfo &info);

/// @brief Expand environment variables in path to respective values
/// @param path The source path to information
/// @return The resulting full path
std::string expand_environment_variables(const std::string &path);

/// @brief Creates the experiment random number generator seed
/// @param job_id Optional batch job identifier
/// @param user_seed User input custom seed for experiment
/// @return The experiment seed, if user provide a seed
std::optional<unsigned int> create_job_seed(int job_id, std::optional<unsigned int> user_seed);
} // namespace hgps::input
