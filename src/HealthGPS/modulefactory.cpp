#include "modulefactory.h"
#include <stdexcept>

namespace hgps {

	SimulationModuleFactory::SimulationModuleFactory(Repository& data_repository)
		:repository_{data_repository} {}

	std::size_t SimulationModuleFactory::size() const noexcept {
		return builders_.size();
	}

	bool SimulationModuleFactory::contains(const SimulationModuleType type) const noexcept {
		return builders_.contains(type);
	}

	void SimulationModuleFactory::register_builder(
		const SimulationModuleType type, const ConcreteBuilder builder) {

		builders_.emplace(type, builder);
	}

	SimulationModuleFactory::ModuleType SimulationModuleFactory::create(
		const SimulationModuleType type, const ModelInput& config) {

		auto it = builders_.find(type);
		if (it != builders_.end()) {
			auto builty_module = it->second(repository_.get(), config);
			return builty_module;
		}

		throw std::invalid_argument{
			"Trying to create a module of unknown type: " + std::to_string((int)type) };
	}
}