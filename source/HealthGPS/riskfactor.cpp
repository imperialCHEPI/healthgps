#include "riskfactor.h"

namespace hgps {

	RiskFactorModule::RiskFactorModule(
		std::unordered_map<HierarchicalModelType, HierarchicalLinearModel>&& models) 
		: models_{models} {

		if (models.empty()) {
			throw std::invalid_argument(
				"Missing required hierarchical model of types = static and dynamic.");
		}

		if (!models.contains(HierarchicalModelType::Static)) {
			throw std::invalid_argument("Missing required hierarchical model of type = static.");
		}

		if (!models.contains(HierarchicalModelType::Dynamic)) {
			throw std::invalid_argument("Missing required hierarchical model of type = dynamic.");
		}
	}

	SimulationModuleType RiskFactorModule::type() const {
		return SimulationModuleType::RiskFactor;
	}

	std::string RiskFactorModule::name() const {
		return "RiskFactor";
	}

	std::size_t RiskFactorModule::size() const noexcept {
		return models_.size();
	}

	HierarchicalLinearModel& RiskFactorModule::operator[](HierarchicalModelType modelType) {
		return models_.at(modelType);
	}

	const HierarchicalLinearModel& RiskFactorModule::operator[](HierarchicalModelType modelType) const {
		return models_.at(modelType);
	}

	void RiskFactorModule::initialise_population(RuntimeContext& context) {
		if (models_.contains(HierarchicalModelType::Static)) {
			models_.at(HierarchicalModelType::Static).generate(context);
		}

		throw std::logic_error("RiskFactorModule has not static hierarchical model.");
	}
}