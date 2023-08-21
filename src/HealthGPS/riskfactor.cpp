#include "riskfactor.h"

namespace hgps {

RiskFactorModule::RiskFactorModule(
    std::map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>> &&models,
    RiskfactorAdjustmentModel &&adjustments)
    : models_{std::move(models)}, adjustment_{std::move(adjustments)} {

    if (models_.empty()) {
        throw std::invalid_argument(
            "Missing required hierarchical model of types = static and dynamic.");
    }

    if (!models_.contains(HierarchicalModelType::Static)) {
        throw std::invalid_argument("Missing required hierarchical model of type = static.");
    }
    if (models_.at(HierarchicalModelType::Static)->type() != HierarchicalModelType::Static) {
        throw std::out_of_range("Model type mismatch in 'static' hierarchical model entry.");
    }

    if (!models_.contains(HierarchicalModelType::Dynamic)) {
        throw std::invalid_argument("Missing required hierarchical model of type = dynamic.");
    }
    if (models_.at(HierarchicalModelType::Dynamic)->type() != HierarchicalModelType::Dynamic) {
        throw std::out_of_range("Model type mismatch in 'dynamic' hierarchical model entry.");
    }
}

SimulationModuleType RiskFactorModule::type() const noexcept {
    return SimulationModuleType::RiskFactor;
}

const std::string &RiskFactorModule::name() const noexcept { return name_; }

std::size_t RiskFactorModule::size() const noexcept { return models_.size(); }

bool RiskFactorModule::contains(const HierarchicalModelType &model_type) const noexcept {
    return models_.contains(model_type);
}

HierarchicalLinearModel &RiskFactorModule::at(const HierarchicalModelType &model_type) const {
    return *models_.at(model_type);
}

void RiskFactorModule::initialise_population(RuntimeContext &context) {
    auto &static_model = models_.at(HierarchicalModelType::Static);
    static_model->generate_risk_factors(context);
}

void RiskFactorModule::update_population(RuntimeContext &context) {
    // Generate risk factors for newborns
    auto &static_model = models_.at(HierarchicalModelType::Static);
    static_model->update_risk_factors(context);

    // Update risk factors for population
    auto &dynamic_model = models_.at(HierarchicalModelType::Dynamic);
    dynamic_model->update_risk_factors(context);
}

void RiskFactorModule::apply_baseline_adjustments(RuntimeContext &context) {
    adjustment_.Apply(context);
}

std::unique_ptr<RiskFactorModule>
build_risk_factor_module(Repository &repository, [[maybe_unused]] const ModelInput &config) {

    auto models = std::map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>{};

    // Static (initialisation) model
    const auto &static_definition =
        repository.get_risk_factor_model_definition(HierarchicalModelType::Static);
    auto static_model = static_definition.create_model();
    models.emplace(HierarchicalModelType::Static, std::move(static_model));

    // Dynamic (update) model
    const auto &dynamic_definition =
        repository.get_risk_factor_model_definition(HierarchicalModelType::Dynamic);
    auto dynamic_model = dynamic_definition.create_model();
    models.emplace(HierarchicalModelType::Dynamic, std::move(dynamic_model));

    auto adjustment_model =
        RiskfactorAdjustmentModel{repository.get_baseline_adjustment_definition()};
    return std::make_unique<RiskFactorModule>(std::move(models), std::move(adjustment_model));
}
} // namespace hgps