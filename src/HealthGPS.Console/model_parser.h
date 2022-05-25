#pragma once
#include "HealthGPS/repository.h"
#include "HealthGPS/hierarchical_model_types.h"
#include "HealthGPS/riskfactor_adjustment_types.h"

#include "options.h"

hgps::BaselineAdjustment load_baseline_adjustments(const BaselineInfo& info);

hgps::HierarchicalLinearModelDefinition load_static_risk_model_definition(std::string model_filename);

hgps::LiteHierarchicalModelDefinition load_dynamic_risk_model_info(std::string model_filename);

void register_risk_factor_model_definitions(const ModellingInfo& info,
	const SettingsInfo& settings, hgps::CachedRepository& repository);