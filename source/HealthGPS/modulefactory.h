#pragma once

#include <stdexcept>
#include <unordered_map>
#include "interfaces.h"

namespace hgps {

	class ModuleFactory
	{
    public:
        using ModuleType = std::unique_ptr<Module>;
        using ConcreteBuilder = ModuleType(*)(hgps::core::Datastore&);

        ModuleFactory() = delete;

        explicit ModuleFactory(hgps::core::Datastore& manager) 
            :manager_{ manager } {}

        void Register(std::string_view kind, ConcreteBuilder builder) {
            builders_.emplace(kind, builder);
        }

        ModuleType Create(std::string_view kind) {
            auto it = builders_.find(kind);
            if (it != builders_.end()) {
                return it->second(manager_);
            }
            else
            {
                throw std::invalid_argument{ "Trying to create a module of unknown kind: " + std::string(kind) };
            }
        }

    private:
        hgps::core::Datastore& manager_;
        std::unordered_map<std::string_view, ConcreteBuilder> builders_;
	};
}

