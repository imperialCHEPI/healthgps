#include "riskfactor.h"
#include "static_linear_model.h"
#include "risk_factor_inspector.h"
#include <set>

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
    auto &static_model = models_.at(RiskFactorModelType::Static);
    static_model->generate_risk_factors(context);

    auto &dynamic_model = models_.at(RiskFactorModelType::Dynamic);
    dynamic_model->generate_risk_factors(context);
    
    // MAHIMA: Write inspection CSV files AFTER both models complete
    // This ensures BMI is calculated and adjustments are applied
    if (context.has_risk_factor_inspector()) {
        auto &inspector = context.get_risk_factor_inspector();
        
        // Only write CSV if debug config is enabled
        if (inspector.is_debug_enabled()) {
            // Get all unique risk factors from all enabled configurations
            std::set<std::string> unique_risk_factors;
            for (const auto &config : inspector.get_debug_configs()) {
                if (config.enabled && !config.target_risk_factor.empty()) {
                    unique_risk_factors.insert(config.target_risk_factor);
                }
            }
            
            // Write CSV for each unique risk factor
            for (const auto &risk_factor : unique_risk_factors) { 
                for (auto &person : context.population()) {
                    if (person.is_active()) {
                        inspector.capture_person_risk_factors(context, person, risk_factor, 0);
                    }
                }
            }
        }
    }
}

void RiskFactorModule::update_population(RuntimeContext &context) {
    // Generate risk factors for newborns
    auto &static_model = models_.at(RiskFactorModelType::Static);
    static_model->update_risk_factors(context);

    // Update risk factors for population
    auto &dynamic_model = models_.at(RiskFactorModelType::Dynamic);
    dynamic_model->update_risk_factors(context);
    
    // MAHIMA: Write inspection CSV files AFTER both models complete
    // This ensures BMI is calculated and adjustments are applied
    if (context.has_risk_factor_inspector()) {
        auto &inspector = context.get_risk_factor_inspector();
        
        // Only write CSV if debug config is enabled
        if (inspector.is_debug_enabled()) {
            // Get all unique risk factors from all enabled configurations
            std::set<std::string> unique_risk_factors;
            for (const auto &config : inspector.get_debug_configs()) {
                if (config.enabled && !config.target_risk_factor.empty()) {
                    unique_risk_factors.insert(config.target_risk_factor);
                }
            }
            
            // Write CSV for each unique risk factor
            for (const auto &risk_factor : unique_risk_factors) {
                for (auto &person : context.population()) {
                    if (person.is_active()) {
                        inspector.capture_person_risk_factors(context, person, risk_factor, 0);
                    }
                }
            }
        }
    }
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

