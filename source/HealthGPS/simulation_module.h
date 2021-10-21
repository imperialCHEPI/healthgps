#pragma once

#include "modulefactory.h"
#include "ses.h"
#include "demographic.h"
#include "riskfactor.h"
#include "disease.h"
#include "disease_table.h"
#include "analysis_module.h"

namespace hgps {

	/// @brief Create the default simulation modules factory for production usage. 
	/// @param manager the data repository manger to be used to create the modules.
	/// @return the default production instance of simulation modules factory.
	static SimulationModuleFactory get_default_simulation_module_factory(Repository& manager) 
	{
		auto factory = SimulationModuleFactory(manager);
		factory.register_builder(SimulationModuleType::SES,
			[](Repository& repository, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_ses_module(repository, config); });

		factory.register_builder(SimulationModuleType::Demographic,
			[](Repository& repository, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_population_module(repository, config); });

		factory.register_builder(SimulationModuleType::RiskFactor,
			[](Repository& repository, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_risk_factor_module(repository, config); });

		factory.register_builder(SimulationModuleType::Disease,
			[](Repository& repository, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_disease_module(repository, config); });

		factory.register_builder(SimulationModuleType::Analysis,
			[](Repository& repository, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
				return build_analysis_module(repository, config); });

		return factory;
	}
}