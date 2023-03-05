#pragma once

#include "interfaces.h"
#include "modelinput.h"
#include "repository.h"
#include <functional>
#include <unordered_map>

namespace hgps {

	/// @brief Defines the SimulationModule factory data type
    ///
    /// @details The module factory is used to assemble the concrete 
    /// modules implementation to compose the Health-GPS microsimulation
    /// instance. The simulation engine will request an instance of each
    /// of the SimulationModuleType enumeration module at construction.
    /// 
    /// The concrete builder function receives an instance of the back-end
    /// data storage and the user simulation inputs to create the respective
    /// module type instance for use by the microsimulation algorithm.
	class SimulationModuleFactory
	{
    public:
        /// @brief Base module type
        using ModuleType = std::shared_ptr<SimulationModule>;

        /// @brief Concrete module builder function signature
        using ConcreteBuilder = ModuleType(*)(Repository&, const ModelInput&);

        SimulationModuleFactory() = delete;
        /// @brief Initialises a new instance of the SimulationModuleFactory class.
        /// @param data_repository The back-end data repository instance to use
        SimulationModuleFactory(Repository& data_repository);

        /// @brief Gets the number of registered module instance
        /// @return NUmber of registered modules
        std::size_t size() const noexcept;

        /// @brief Determine whether the factory contains an instance of a module type
        /// @param type The simulation module type
        /// @return true, if the factory contains a instance; otherwise, false
        bool contains(const SimulationModuleType type) const noexcept;

        /// @brief Registers a simulation module type builder function 
        /// @param type The simulation module type
        /// @param builder The module type instance builder function
        void register_builder(const SimulationModuleType type, const ConcreteBuilder builder);

        /// @brief Create a polymorphic SimulationModule instance 
        /// @param type The simulation module type
        /// @param config The simulation model inputs definition
        /// @return A new SimulationModule instance
        ModuleType create(const SimulationModuleType type, const ModelInput& config);

    private:
        std::reference_wrapper<Repository> repository_;
        std::unordered_map<SimulationModuleType, ConcreteBuilder> builders_;
	};
}
