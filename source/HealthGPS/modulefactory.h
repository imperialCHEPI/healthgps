#pragma once

#include <stdexcept>
#include <unordered_map>
#include "interfaces.h"
#include <HealthGPS.Core/poco.h>

namespace hgps {

	class SimulationModuleFactory
	{
    public:
        using ModuleType = std::unique_ptr<SimulationModule>;
        using ConcreteBuilder = ModuleType(*)(core::Datastore&, core::Country&);

        SimulationModuleFactory() = delete;

        explicit SimulationModuleFactory(core::Datastore& manager) 
            :manager_{ manager } {}

        void Register(SimulationModuleType type, ConcreteBuilder builder) {
            builders_.emplace(type, builder);
        }

        ModuleType Create(SimulationModuleType type, core::Country& country) {
            auto it = builders_.find(type);
            if (it != builders_.end()) {
                return it->second(manager_, country);
            }
            else
            {
                throw std::invalid_argument{
                    "Trying to create a module of unknown type: " + std::to_string((int)type) };
            }
        }

    private:
        hgps::core::Datastore& manager_;
        std::unordered_map<SimulationModuleType, ConcreteBuilder> builders_;
	};
}

