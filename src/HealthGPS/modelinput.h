#pragma once

#include <HealthGPS.Core/datatable.h>
#include <HealthGPS.Core/string_util.h>
#include <map>

#include "mapping.h"
#include "settings.h"
#include "HealthGPS.Input/poco.h"

namespace hgps {

/// @brief Defines the Simulation run information data type
struct RunInfo {
    /// @brief Experiment start time
    unsigned int start_time{};

    /// @brief Experiment stop time
    unsigned int stop_time{};

    /// @brief Scenarios data synchronisation timeout in milliseconds
    unsigned int sync_timeout_ms{};

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

    /// @brief Initialise a new instance of the ModelInput class.
    /// @param data The risk factors fitted dataset
    /// @param settings Experiment settings definition
    /// @param run_info Simulation run information
    /// @param ses_info Socio-economic status (SES) model information
    /// @param risk_mapping Hierarchical risk factors model mappings
    /// @param diseases Selected diseases to include in experiment
    /// @param pif_info Population Impact Fraction configuration
    ModelInput(core::DataTable &data, Settings settings, const RunInfo &run_info,
               SESDefinition ses_info, HierarchicalMapping risk_mapping,
               std::vector<core::DiseaseInfo> diseases, hgps::input::PIFInfo pif_info = hgps::input::PIFInfo{});

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

    /// @brief Gets whether income-based analysis is enabled
    /// @return true if income analysis is enabled, false otherwise
    bool enable_income_analysis() const noexcept;

    /// @brief Gets the Population Impact Fraction configuration
    /// @return PIF configuration
    const hgps::input::PIFInfo& population_impact_fraction() const noexcept;

  private:
    std::reference_wrapper<core::DataTable> input_data_;
    Settings settings_;
    RunInfo run_info_;
    SESDefinition ses_definition_;
    HierarchicalMapping risk_mapping_;
    std::vector<core::DiseaseInfo> diseases_;
    bool enable_income_analysis_{true}; // This is to set if results be categorised by income or not. Set to TRUE for now.
    hgps::input::PIFInfo pif_info_;
};
} // namespace hgps
