#pragma once

#include <stdexcept>
#include <unordered_map>
#include "interfaces.h"
#include "modelinput.h"
#include "repository.h"

namespace hgps {

	class SimulationModuleFactory
	{
    public:
        using ModuleType = std::shared_ptr<SimulationModule>;
        using ConcreteBuilder = ModuleType(*)(core::Datastore&, const ModelInput&);

        SimulationModuleFactory() = delete;
        SimulationModuleFactory(Repository& data_repository);

        std::size_t size() const noexcept;

        bool countains(const SimulationModuleType type) const noexcept;

        void register_instance(const SimulationModuleType type, const ModuleType instance);

        void register_builder(const SimulationModuleType type, const ConcreteBuilder builder);

        ModuleType create(const SimulationModuleType type, const ModelInput& config);

    private:
        Repository& repository_;
        std::unordered_map<SimulationModuleType, ConcreteBuilder> builders_;
        std::map<SimulationModuleType, std::weak_ptr<SimulationModule>> registry_;
	};
}
