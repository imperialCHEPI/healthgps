#include "simulation_module.h"

namespace hgps {

SimulationModuleFactory get_default_simulation_module_factory(Repository &manager) 
{
    auto factory = SimulationModuleFactory(manager);

    factory.register_builder(SimulationModuleType::SES,
                             [](Repository &repository, const ModelInput &config) -> SimulationModuleFactory::ModuleType 
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
