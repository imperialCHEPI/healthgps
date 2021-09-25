#pragma once

#include "modulefactory.h"
#include "ses.h"
#include "demographic.h"
#include "riskfactor.h"
#include "disease.h"
#include "disease_table.h"
#include "analysis_module.h"
#include "countrymodule.h"

namespace hgps {

	/// @brief Create the default simulation modules factory for production usage. 
	/// @param manager the data repository manger to be used to create the modules.
	/// @return the default production instance of simulation modules factory.
	static SimulationModuleFactory get_default_simulation_module_factory(Repository& manager) 
	{
		auto factory = SimulationModuleFactory(manager);
		factory.register_builder(SimulationModuleType::SES,
			[](core::Datastore& manager, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_ses_module(manager, config); });

		factory.register_builder(SimulationModuleType::Demographic,
			[](core::Datastore& manager, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_demographic_module(manager, config); });

		factory.register_builder(SimulationModuleType::Disease,
			[](core::Datastore& manager, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_disease_module(manager, config); });

		factory.register_builder(SimulationModuleType::Analysis,
			[](core::Datastore& manager, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_analysis_module(manager, config); });

		return factory;
	}
}