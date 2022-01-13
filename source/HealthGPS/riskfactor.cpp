#include "riskfactor.h"
#include "hierarchical_model_registry.h"

namespace hgps {

	RiskFactorModule::RiskFactorModule(
		std::unordered_map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>&& models)
		: models_{ std::move(models) } {

		if (models_.empty()) {
			throw std::invalid_argument(
				"Missing required hierarchical model of types = static and dynamic.");
		}

		if (!models_.contains(HierarchicalModelType::Static)) {
			throw std::invalid_argument("Missing required hierarchical model of type = static.");
		}
		else if (models_.at(HierarchicalModelType::Static)->type() != HierarchicalModelType::Static) {
			throw std::invalid_argument("Model type mismatch in 'static' hierarchical model entry.");
		}

		if (!models_.contains(HierarchicalModelType::Dynamic)) {
			throw std::invalid_argument("Missing required hierarchical model of type = dynamic.");
		}
		else if (models_.at(HierarchicalModelType::Dynamic)->type() != HierarchicalModelType::Dynamic) {
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

	bool RiskFactorModule::contains(const HierarchicalModelType& model_type) const noexcept {
		return models_.contains(model_type);
	}

	HierarchicalLinearModel& RiskFactorModule::at(const HierarchicalModelType& model_type) const {
		return *models_.at(model_type);
	}

	void RiskFactorModule::initialise_population(RuntimeContext& context) {
		auto& static_model = models_.at(HierarchicalModelType::Static);
		static_model->generate_risk_factors(context);

		// This should be a internal function called by generate_risk_factors?
		// static_model->adjust_risk_factors_with_baseline(context);
	}

	void RiskFactorModule::update_population(RuntimeContext& context) {
		// Generate risk factors for newborns
		auto& static_model = models_.at(HierarchicalModelType::Static);
		static_model->update_risk_factors(context);

		// Update risk factors for population
		auto& dynamic_model = models_.at(HierarchicalModelType::Dynamic);
		dynamic_model->update_risk_factors(context);
	}

	std::unique_ptr<RiskFactorModule> build_risk_factor_module(Repository& repository, [[maybe_unused]] const ModelInput& config)
	{
		// Both model types are required, and must be registered
		auto full_registry = get_default_hierarchical_model_registry();
		auto lite_registry = get_default_lite_hierarchical_model_registry();

		auto models = std::unordered_map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>{};
		if (full_registry.contains(HierarchicalModelType::Static)) {
			models.emplace(HierarchicalModelType::Static, full_registry.at(HierarchicalModelType::Static)(
				repository.get_linear_model_definition(HierarchicalModelType::Static)));
		}
		else {
			models.emplace(HierarchicalModelType::Static, lite_registry.at(HierarchicalModelType::Static)(
				repository.get_lite_linear_model_definition(HierarchicalModelType::Static)));
		}

		// Creates dynamic, must be in one of the registries
		if (full_registry.contains(HierarchicalModelType::Dynamic)) {
			models.emplace(HierarchicalModelType::Dynamic, full_registry.at(HierarchicalModelType::Dynamic)(
				repository.get_linear_model_definition(HierarchicalModelType::Dynamic)));
		}
		else {
			models.emplace(HierarchicalModelType::Dynamic, lite_registry.at(HierarchicalModelType::Dynamic)(
				repository.get_lite_linear_model_definition(HierarchicalModelType::Dynamic)));
		}

		return std::make_unique<RiskFactorModule>(std::move(models));
	}
}