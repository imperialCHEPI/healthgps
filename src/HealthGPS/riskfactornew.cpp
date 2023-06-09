#include "riskfactornew.h"
#include "hierarchical_model_registry.h"

namespace hgps {

	RiskFactorModuleNew::RiskFactorModuleNew(
		std::map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>&& models,
		RiskfactorAdjustmentModel&& adjustments)
		: models_{ std::move(models) }, adjustment_{ std::move(adjustments) } {

		if (models_.empty()) {
			throw std::invalid_argument(
				"Missing required hierarchical model of types = static and dynamic.");
		}

		if (!models_.contains(HierarchicalModelType::Static)) {
			throw std::invalid_argument("Missing required hierarchical model of type = static.");
		}
		else if (models_.at(HierarchicalModelType::Static)->type() != HierarchicalModelType::Static) {
			throw std::out_of_range("Model type mismatch in 'static' hierarchical model entry.");
		}

		if (!models_.contains(HierarchicalModelType::Dynamic)) {
			throw std::invalid_argument("Missing required hierarchical model of type = dynamic.");
		}
		else if (models_.at(HierarchicalModelType::Dynamic)->type() != HierarchicalModelType::Dynamic) {
			throw std::out_of_range("Model type mismatch in 'dynamic' hierarchical model entry.");
		}
	}

	SimulationModuleType RiskFactorModuleNew::type() const noexcept {
		return SimulationModuleType::RiskFactor;
	}

	const std::string& RiskFactorModuleNew::name() const noexcept {
		return name_;
	}

	std::size_t RiskFactorModuleNew::size() const noexcept {
		return models_.size();
	}

	bool RiskFactorModuleNew::contains(const HierarchicalModelType& model_type) const noexcept {
		return models_.contains(model_type);
	}

	HierarchicalLinearModel& RiskFactorModuleNew::at(const HierarchicalModelType& model_type) const {
		return *models_.at(model_type);
	}

	void RiskFactorModuleNew::initialise_population(RuntimeContext& context) {
		auto& static_model = models_.at(HierarchicalModelType::Static);
		static_model->generate_risk_factors(context);
	}

	void RiskFactorModuleNew::update_population(RuntimeContext& context) {
		// Generate risk factors for newborns
		auto& static_model = models_.at(HierarchicalModelType::Static);
		static_model->update_risk_factors(context);

		// Update risk factors for population
		auto& dynamic_model = models_.at(HierarchicalModelType::Dynamic);
		dynamic_model->update_risk_factors(context);
	}

	void RiskFactorModuleNew::apply_baseline_adjustments(RuntimeContext& context)
	{
		adjustment_.Apply(context);
	}

	std::unique_ptr<RiskFactorModuleNew> build_risk_factor_module(Repository& repository, [[maybe_unused]] const ModelInput& config)
	{
		// Both model types are required, and must be registered
		auto full_registry = get_default_hierarchical_model_registry();
		auto lite_registry = get_default_lite_hierarchical_model_registry();

		auto models = std::map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>{};
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

		auto adjustment_model = RiskfactorAdjustmentModel{ repository.get_baseline_adjustment_definition()};
		return std::make_unique<RiskFactorModuleNew>(std::move(models), std::move(adjustment_model));
	}
}
