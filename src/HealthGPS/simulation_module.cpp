#include "simulation_module.h"

namespace hgps {

SimulationModuleFactory get_default_simulation_module_factory(Repository &manager) 
{
    // Function creates and configures a SimulationModuleFactory object.

    auto factory = SimulationModuleFactory(manager); //This creates a SimulationModuleFactory object named factory, passing the manager object to its constructor.  This likely initializes the factory with the necessary dependencies.

    factory.register_builder(SimulationModuleType::SES,
                             [](Repository &repository, const ModelInput &config) -> SimulationModuleFactory::ModuleType // Lambda expressions acts as a builder function for a specific module type. They take a Repository and a ModelInput object as input and return a SimulationModuleFactory::ModuleType object.
                                {
                                    return build_ses_noise_module(repository, config);
                                });

    factory.register_builder(SimulationModuleType::Demographic,
                             [](Repository &repository, const ModelInput &config) -> SimulationModuleFactory::ModuleType 
                                {
                                    return build_population_module(repository, config);
                                });

    factory.register_builder(SimulationModuleType::RiskFactor,
                             [](Repository &repository, const ModelInput &config) -> SimulationModuleFactory::ModuleType 
                                {
                                    return build_risk_factor_module(repository, config);
                                });

    factory.register_builder(SimulationModuleType::Disease,
                             [](Repository &repository, const ModelInput &config) -> SimulationModuleFactory::ModuleType 
                                {
                                    return build_disease_module(repository, config);
                                });

    factory.register_builder(SimulationModuleType::Analysis,
                             [](Repository &repository, const ModelInput &config) -> SimulationModuleFactory::ModuleType
                                {
                                    return build_analysis_module(repository, config);
                                });
    return factory;
}
} // namespace hgps
