#include "riskfactor.h"

namespace hgps {

	RiskFactorModule::RiskFactorModule(
		std::unordered_map<HierarchicalModelType, std::shared_ptr<HierarchicalLinearModel>>&& models) 
		: models_{models} {

		if (models.empty()) {
			throw std::invalid_argument(
				"Missing required hierarchical model of types = static and dynamic.");
		}

		if (!models.contains(HierarchicalModelType::Static)) {
			throw std::invalid_argument("Missing required hierarchical model of type = static.");
		}
		else if (models.at(HierarchicalModelType::Static)->type() != HierarchicalModelType::Static) {
			throw std::invalid_argument("Model type mismatch in 'static' hierarchical model entry.");
		}

		if (!models.contains(HierarchicalModelType::Dynamic)) {
			throw std::invalid_argument("Missing required hierarchical model of type = dynamic.");
		}
		else if (models.at(HierarchicalModelType::Dynamic)->type() != HierarchicalModelType::Dynamic) {
			throw std::invalid_argument("Model type mismatch in 'dynamic' hierarchical model entry.");
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

	std::shared_ptr<HierarchicalLinearModel> RiskFactorModule::operator[](HierarchicalModelType modelType) {
		return models_.at(modelType);
	}

	const std::shared_ptr<HierarchicalLinearModel> RiskFactorModule::operator[](HierarchicalModelType modelType) const {
		return models_.at(modelType);
	}

	void RiskFactorModule::initialise_population(RuntimeContext& context) {
		models_.at(HierarchicalModelType::Static)->generate(context);
	}
}