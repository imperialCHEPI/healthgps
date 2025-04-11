#pragma once

#include <HealthGPS.Core/datatable.h>
#include <HealthGPS.Core/string_util.h>
#include <map>

#include "mapping.h"
#include "settings.h"

namespace hgps {

/// @brief Defines the Simulation run information data type
struct RunInfo {
    /// @brief Experiment start time
    unsigned int start_time{};

    /// @brief Experiment stop time
    unsigned int stop_time{};

    /// @brief Scenarios data synchronisation timeout in milliseconds
    unsigned int sync_timeout_ms{120000};

    /// @brief Custom seed to initialise the pseudo-number generator engine
    std::optional<unsigned int> seed{};

    /// @brief The application logging verbosity
    core::VerboseMode verbosity{};

    /// @brief Maximum number of comorbidities to include in results
    unsigned int comorbidities{};
};

/// @brief Defines the socio-economic status (SES) model data type
struct SESDefinition {
    /// @brief Socio-economic status (SES) function identification
    std::string fuction_name;

    /// @brief The SES model function parameters
    std::vector<double> parameters;
};

/// @brief Defines the Simulation model inputs data type
class ModelInput {
  public:
    ModelInput() = delete;

    /// @brief Constructs a new model input instance.
    /// @param data The input data table
    /// @param settings The model settings
    /// @param run_info The run information
    /// @param ses_info The socioeconomic status definition
    /// @param risk_mapping The risk mapping
    /// @param diseases The diseases information
    ModelInput(core::DataTable data, Settings settings, const RunInfo &run_info,
               SESDefinition ses_info, HierarchicalMapping risk_mapping,
               std::vector<core::DiseaseInfo> diseases);

    /// @brief Gets the simulation experiment settings definition
    /// @return Experiment settings definition
    const Settings &settings() const noexcept;

    /// @brief Gets the risk factors model dataset
    /// @return Risk factors dataset
    const core::DataTable &data() const noexcept;

    /// @brief Gets the experiment start time
    /// @return Experiment start time
    unsigned int start_time() const noexcept;

    /// @brief Gets the experiment stop time
    /// @return Experiment stop time
    unsigned int stop_time() const noexcept;

    /// @brief Gets the scenarios data synchronisation timeout (milliseconds)
    /// @return Scenarios synchronisation timeout
    unsigned int sync_timeout_ms() const noexcept;

    /// @brief Gets the user custom seed to initialise the pseudo-number generator
    /// @return User custom seed value, if provide; otherwise empty.
    const std::optional<unsigned int> &seed() const noexcept;

    /// @brief Gets the simulation run information
    /// @return simulation run information
    const RunInfo &run() const noexcept;

    /// @brief Gets the Socio-economic status (SES) model information
    /// @return SES model information
    const SESDefinition &ses_definition() const noexcept;

    /// @brief Gets the Hierarchical risk factors model mappings definition
    /// @return Risk factors model mappings
    const HierarchicalMapping &risk_mapping() const noexcept;

    /// @brief Gets the collection of diseases to include in experiment
    /// @return Diseases to include in experiment
    const std::vector<core::DiseaseInfo> &diseases() const noexcept;

    /// @brief Get region probabilities for specific age and gender stratum
    /// @param age The age stratum
    /// @param gender The gender stratum
    /// @return Map of region to probability for this stratum
    std::unordered_map<core::Region, double> get_region_probabilities(int age,
                                                                      core::Gender gender) const;

    /// @brief Get ethnicity probabilities for specific age, gender, and region stratum
    /// @param age The age stratum
    /// @param gender The gender stratum
    /// @param region The region stratum
    /// @return Map of ethnicity to probability for this stratum
    std::unordered_map<core::Ethnicity, double>
    get_ethnicity_probabilities(int age, core::Gender gender, core::Region region) const;

  private:
    core::DataTable input_data_;
    Settings settings_;
    RunInfo run_info_;
    SESDefinition ses_definition_;
    HierarchicalMapping risk_mapping_;
    std::vector<core::DiseaseInfo> diseases_;
};
} // namespace hgps
