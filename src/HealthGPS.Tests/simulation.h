#pragma once
#include "data_config.h"

#include "HealthGPS.Core/column_builder.h"
#include "HealthGPS.Input/api.h"
#include "HealthGPS/analysis_module.h"
#include "HealthGPS/api.h"
#include "HealthGPS/converter.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/random_algorithm.h"
#include "RiskFactorData.h"

#include <atomic>
#include <map>
#include <optional>

namespace {
std::vector<hgps::MappingEntry> mapping_entries{
    {{"Gender", 0}, {"Age", 0}, {"SmokingStatus", 1}, {"AlcoholConsumption", 1}, {"BMI", 2}}};
} // anonymous namespace

void create_test_datatable(hgps::core::DataTable &data);

hgps::ModelInput create_test_configuration(hgps::core::DataTable &data);
