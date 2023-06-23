#pragma once

#include "analysis_module.h"
#include "demographic.h"
#include "disease.h"
#include "disease_table.h"
#include "modulefactory.h"
#include "riskfactor.h"
#include "ses_noise_module.h"

namespace hgps {

/// @brief Create the default simulation modules factory for production usage.
/// @param manager the data repository manger to be used to create the modules.
/// @return the default production instance of simulation modules factory.
SimulationModuleFactory get_default_simulation_module_factory(Repository &manager);
} // namespace hgps