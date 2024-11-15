#pragma once
#include "data_config.h"

#include "HealthGPS.Core/column_builder.h"
#include "HealthGPS.Input/api.h"
#include "HealthGPS/analysis_module.h"
#include "HealthGPS/api.h"
#include "HealthGPS/converter.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/random_algorithm.h"

#include <atomic>
#include <map>
#include <optional>

void create_test_datatable(hgps::core::DataTable &data);

std::shared_ptr<hgps::ModelInput> create_test_configuration(hgps::core::DataTable &data);
