#include "riskfactor.h"
#include <iostream>

namespace hgps {

RiskFactorModule::RiskFactorModule(
    std::map<RiskFactorModelType, std::unique_ptr<RiskFactorModel>> models)
    : models_{std::move(models)} {

    if (models_.empty()) {
        throw std::invalid_argument("Missing required model of types = static and dynamic.");
    }

    if (!models_.contains(RiskFactorModelType::Static)) {
        throw std::invalid_argument("Missing required model of type = static.");
    }
    if (models_.at(RiskFactorModelType::Static)->type() != RiskFactorModelType::Static) {
        throw std::out_of_range("Model type mismatch in 'static' model entry.");
    }

    if (!models_.contains(RiskFactorModelType::Dynamic)) {
        throw std::invalid_argument("Missing required model of type = dynamic.");
    }
    if (models_.at(RiskFactorModelType::Dynamic)->type() != RiskFactorModelType::Dynamic) {
        throw std::out_of_range("Model type mismatch in 'dynamic' model entry.");
    }
}

SimulationModuleType RiskFactorModule::type() const noexcept {
    return SimulationModuleType::RiskFactor;
}

const std::string &RiskFactorModule::name() const noexcept { return name_; }

std::size_t RiskFactorModule::size() const noexcept { return models_.size(); }

bool RiskFactorModule::contains(const RiskFactorModelType &model_type) const noexcept {
    return models_.contains(model_type);
}

RiskFactorModel &RiskFactorModule::at(const RiskFactorModelType &model_type) const {
    return *models_.at(model_type);
}

void RiskFactorModule::initialise_population(RuntimeContext &context) {
    std::cout << "\nDEBUG: RiskFactorModule::initialise_population called - START";
    if (!models_.contains(RiskFactorModelType::Static)) {
        std::cout << " ERROR: Static model not found!";
        throw std::runtime_error("Static model not found in RiskFactorModule");
    }
    try {
        auto &static_model = models_.at(RiskFactorModelType::Static);

        std::cout << "\nDEBUG: Calling static_model->generate_risk_factors...";
        static_model->generate_risk_factors(context);
        std::cout << " OK, static model generate_risk_factors completed";
    } catch (const std::exception &e) {
        std::cout << " ERROR: Exception in static model: " << e.what();
        throw;
    }

    std::cout << "\nDEBUG: Checking if dynamic model exists...";
    if (!models_.contains(RiskFactorModelType::Dynamic)) {
        std::cout << " ERROR: Dynamic model not found!";
        throw std::runtime_error("Dynamic model not found in RiskFactorModule");
    }
    std::cout << " OK, dynamic model exists";

    std::cout << "\nDEBUG: Getting dynamic model...";
    try {
        auto &dynamic_model = models_.at(RiskFactorModelType::Dynamic);
        std::cout << " OK, dynamic model obtained";

        std::cout << "\nDEBUG: Calling dynamic_model->generate_risk_factors...";
        dynamic_model->generate_risk_factors(context);
        std::cout << " OK, dynamic model generate_risk_factors completed";
    } catch (const std::exception &e) {
        std::cout << " ERROR: Exception in dynamic model: " << e.what();
        throw;
    }

    std::cout << "\nDEBUG: RiskFactorModule::initialise_population completed successfully";
}

void RiskFactorModule::update_population(RuntimeContext &context) {
    // Generate risk factors for newborns
    auto &static_model = models_.at(RiskFactorModelType::Static);
    static_model->update_risk_factors(context);

    // Update risk factors for population
    auto &dynamic_model = models_.at(RiskFactorModelType::Dynamic);
    dynamic_model->update_risk_factors(context);
}

std::unique_ptr<RiskFactorModule>
build_risk_factor_module(Repository &repository, [[maybe_unused]] const ModelInput &config) {

    auto models = std::map<RiskFactorModelType, std::unique_ptr<RiskFactorModel>>{};

    // Static (initialisation) model
    const auto &static_definition =
        repository.get_risk_factor_model_definition(RiskFactorModelType::Static);
    auto static_model = static_definition.create_model();
    models.emplace(RiskFactorModelType::Static, std::move(static_model));

    // Dynamic (update) model
    const auto &dynamic_definition =
        repository.get_risk_factor_model_definition(RiskFactorModelType::Dynamic);
    auto dynamic_model = dynamic_definition.create_model();
    models.emplace(RiskFactorModelType::Dynamic, std::move(dynamic_model));

    return std::make_unique<RiskFactorModule>(std::move(models));
}

} // namespace hgps
