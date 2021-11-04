#pragma once
#include <cxxopts.hpp>

#include "HealthGPS/modelinput.h"
#include "HealthGPS/scenario.h"
#include "HealthGPS/Intervention_scenario.h"

#include "HealthGPS.Core/api.h"

#include "options.h"

std::string getTimeNowStr();

cxxopts::Options create_options();

CommandOptions parse_arguments(cxxopts::Options& options, int& argc, char* argv[]);

Configuration load_configuration(CommandOptions& options);

std::vector<hgps::core::DiseaseInfo> get_diseases(hgps::core::Datastore& data_api, Configuration& config);

hgps::ModelInput create_model_input(hgps::core::DataTable& input_table, hgps::core::Country country,
	Configuration& config, std::vector<hgps::core::DiseaseInfo> diseases);

std::string create_output_file_name(const ResultInfo& info);

hgps::InterventionScenario create_intervention_scenario(hgps::SyncChannel& channel, const PolicyScenarioInfo& info);
