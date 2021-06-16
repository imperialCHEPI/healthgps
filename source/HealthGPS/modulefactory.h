#pragma once

#include <stdexcept>
#include <unordered_map>
#include "interfaces.h"

namespace hgps {

	class ModuleFactory
	{
    public:
        using ModuleInstance = std::unique_ptr<Module>;
        using ConcreteBuilder = ModuleInstance(*)(hgps::core::Datastore&);

        ModuleFactory() = delete;

        explicit ModuleFactory(hgps::core::Datastore& manager) 
            :manager_{ manager } {}

        void Register(ModuleType type, ConcreteBuilder builder) {
            builders_.emplace(type, builder);
        }

        ModuleInstance Create(ModuleType type) {
            auto it = builders_.find(type);
            if (it != builders_.end()) {
                return it->second(manager_);
            }
            else
            {
                throw std::invalid_argument{
                    "Trying to create a module of unknown type: " + std::to_string((int)type) };
            }
        }

    private:
        hgps::core::Datastore& manager_;
        std::unordered_map<ModuleType, ConcreteBuilder> builders_;
	};
}

