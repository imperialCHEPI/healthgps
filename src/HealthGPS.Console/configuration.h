#pragma once
#include <cxxopts.hpp>

#include "HealthGPS/modelinput.h"
#include "HealthGPS/scenario.h"
#include "HealthGPS/intervention_scenario.h"
#include "HealthGPS/healthgps.h"

#include "HealthGPS.Core/api.h"

#include "result_file_writer.h"
#include "options.h"

std::string get_time_now_str();

cxxopts::Options create_options();

void print_app_title();

CommandOptions parse_arguments(cxxopts::Options& options, int& argc, char* argv[]);

Configuration load_configuration(CommandOptions& options);

std::vector<hgps::core::DiseaseInfo> get_diseases(hgps::core::Datastore& data_api, Configuration& config);

hgps::ModelInput create_model_input(hgps::core::DataTable& input_table, hgps::core::Country country,
	Configuration& config, std::vector<hgps::core::DiseaseInfo> diseases);

std::string create_output_file_name(const OutputInfo& info, int job_id);

ResultFileWriter create_results_file_logger(const Configuration& config, const hgps::ModelInput& input);

std::unique_ptr<hgps::Scenario> create_baseline_scenario(hgps::SyncChannel& channel, const PolicyScenarioInfo& info);

hgps::HealthGPS create_baseline_simulation(hgps::SyncChannel& channel, hgps::SimulationModuleFactory& factory,
	hgps::EventAggregator& event_bus, hgps::ModelInput& input, const PolicyScenarioInfo& info);

std::unique_ptr<hgps::InterventionScenario> create_intervention_scenario(
	hgps::SyncChannel& channel, const PolicyScenarioInfo& info);

hgps::HealthGPS create_intervention_simulation(hgps::SyncChannel& channel, hgps::SimulationModuleFactory& factory,
	hgps::EventAggregator& event_bus, hgps::ModelInput& input, const PolicyScenarioInfo& info);

std::string expand_environment_variables(const std::string& path);

std::optional<unsigned int> create_job_seed(int job_id, std::optional<unsigned int> user_seed);
