#include "modulefactory.h"

namespace hgps {

	SimulationModuleFactory::SimulationModuleFactory(Repository& data_repository)
		:repository_{data_repository} {}

	std::size_t SimulationModuleFactory::size() const noexcept {
		return builders_.size() + registry_.size();
	}

	bool SimulationModuleFactory::countains(const SimulationModuleType type) const noexcept {
		return registry_.contains(type) || builders_.contains(type);
	}

	void SimulationModuleFactory::register_instance(
		const SimulationModuleType type, const ModuleType instance) {

		registry_[type] = instance;
	}

	void SimulationModuleFactory::register_builder(
		const SimulationModuleType type, const ConcreteBuilder builder) {

		builders_.emplace(type, builder);
	}

	SimulationModuleFactory::ModuleType SimulationModuleFactory::create(
		const SimulationModuleType type, const ModelInput& config) {

		auto reg_it = registry_.find(type);
		if (reg_it != registry_.end()) {
			if (auto ptr = reg_it->second.lock()) {
				return ptr;
			}

			// Expired.
			registry_.erase(reg_it);
		}

		auto it = builders_.find(type);
		if (it != builders_.end()) {
			auto builty_module = it->second(repository_.manager(), config);
			registry_[type] = builty_module;
			return builty_module;
		}

		throw std::invalid_argument{
			"Trying to create a module of unknown type: " + std::to_string((int)type) };
	}
}