#include "riskfactor.h"

namespace hgps {

	RiskFactorModule::RiskFactorModule(
		std::unordered_map<HierarchicalModelType, HierarchicalLinearModel>&& models) 
		: models_{models} {}

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

		throw std::logic_error("Function not implemented.");
	}
}