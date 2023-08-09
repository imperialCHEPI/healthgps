#pragma once
#include "HealthGPS.Core/forward_type.h"
#include "poco.h"
#include <filesystem>

namespace host {
/// @brief Defines the Command Line Interface (CLI) arguments options
struct CommandOptions {
    /// @brief Indicates whether the argument parsing succeed
    bool success{};

    /// @brief The exit code to return, in case of CLI arguments parsing failure
    int exit_code{};

    /// @brief The configuration file argument value
    std::filesystem::path config_file{};

    /// @brief The back-end storage full path argument value
    std::filesystem::path storage_folder{};

    /// @brief Indicates whether the application logging is verbose
    bool verbose{};

    /// @brief The batch job identifier value, optional.
    int job_id{};
};

/// @brief Defines the application configuration data structure
struct Configuration {
    /// @brief The root path for configuration files
    std::filesystem::path root_path;

    /// @brief The input data file details
    poco::FileInfo file;

    /// @brief Experiment population settings
    poco::SettingsInfo settings;

    /// @brief Socio-economic status (SES) model inputs
    poco::SESInfo ses;

    /// @brief User defined model and parameters information
    poco::ModellingInfo modelling;

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

    /// @brief Indicates whether an alternative intervention policy is active
    bool has_active_intervention{false};

    /// @brief The active intervention policy definition
    poco::PolicyScenarioInfo intervention;

    /// @brief Experiment output folder and file information
    poco::OutputInfo output;

    /// @brief Application logging verbosity mode
    hgps::core::VerboseMode verbosity{};

    /// @brief Experiment batch job identifier
    int job_id{};

    /// @brief Experiment model name
    std::string app_name;

    /// @brief Experiment model version
    std::string app_version;
};
} // namespace host