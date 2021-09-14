#include "riskfactor.h"

namespace hgps {

	RiskFactorModule::RiskFactorModule(
		std::unordered_map<HierarchicalModelType, std::shared_ptr<HierarchicalLinearModel>>&& models)
		: models_{ models } {

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

	SimulationModuleType RiskFactorModule::type() const noexcept {
		return SimulationModuleType::RiskFactor;
	}

	std::string RiskFactorModule::name() const noexcept {
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
		auto static_model = models_.at(HierarchicalModelType::Static);
		static_model->generate_risk_factors(context);

		// This should be a internal function called by generate_risk_factors?
		static_model->adjust_risk_factors_with_baseline(context);
	}

	void RiskFactorModule::update_population(RuntimeContext& context) {
		auto dynamic_model = models_.at(HierarchicalModelType::Dynamic);

		// Generate risk factors for newborns
		dynamic_model->generate_risk_factors(context);

		// Update risk factors for population
		dynamic_model->update_risk_factors(context);
	}
}