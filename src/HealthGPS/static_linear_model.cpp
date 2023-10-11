#include "static_linear_model.h"
#include "runtime_context.h"

#include "HealthGPS.Core/exception.h"

namespace hgps {

StaticLinearModel::StaticLinearModel(std::vector<core::Identifier> risk_factor_names,
                                     LinearModelParams risk_factor_models,
                                     Eigen::MatrixXd risk_factor_cholesky)
    : risk_factor_names_{std::move(risk_factor_names)},
      risk_factor_models_{std::move(risk_factor_models)},
      risk_factor_cholesky_{std::move(risk_factor_cholesky)} {

    if (risk_factor_names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (risk_factor_models_.intercepts.empty() || risk_factor_models_.coefficients.empty()) {
        throw core::HgpsException("Risk factor models mapping is incomplete");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
}

HierarchicalModelType StaticLinearModel::type() const noexcept {
    return HierarchicalModelType::Static;
}

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    for (auto &person : context.population()) {

        // Approximate risk factor values with linear models.
        linear_approximation(person);

        // Correlated residual sampling.
        auto samples = correlated_samples(context);

        // TODO: add residuals to risk factor values
    }
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {

    for (auto &person : context.population()) {

        // Do not initialise non-newborns.
        if (person.age > 0) {
            continue;
        }

        // Approximate risk factor values with linear models.
        linear_approximation(person);

        // Correlated residual sampling.
        auto samples = correlated_samples(context);

        // TODO: add residuals to risk factor values
    }
}

Eigen::VectorXd StaticLinearModel::correlated_samples(RuntimeContext &context) {

    // Correlated samples using Cholesky decomposition.
    Eigen::VectorXd samples{risk_factor_names_.size()};
    for (size_t i = 0; i < risk_factor_names_.size(); i++) {
        samples[i] = context.random().next_normal(0.0, 1.0);
    }
    samples = risk_factor_cholesky_ * samples;

    // Bound and scale the samples.
    double sigma_cap = 3.0; // for removing outliers
    for (size_t i = 0; i < risk_factor_names_.size(); i++) {
        samples[i] = std::min(std::max(samples[i], -sigma_cap), sigma_cap);
        samples[i] = samples[i] / sigma_cap;
    }

    return samples;
}

void StaticLinearModel::linear_approximation(Person &person) {

    // Approximate risk factor values for person with linear models.
    for (const auto &factor_name : risk_factor_names_) {
        double factor = risk_factor_models_.intercepts.at(factor_name);
        const auto &coefficients = risk_factor_models_.coefficients.at(factor_name);

        for (const auto &[coefficient_name, coefficient_value] : coefficients) {
            factor += coefficient_value * person.get_risk_factor_value(coefficient_name);
        }

        person.risk_factors[factor_name] = factor;
    }
}

StaticLinearModelDefinition::StaticLinearModelDefinition(
    std::vector<core::Identifier> risk_factor_names, LinearModelParams risk_factor_models,
    Eigen::MatrixXd risk_factor_cholesky)
    : risk_factor_names_{std::move(risk_factor_names)},
      risk_factor_models_{std::move(risk_factor_models)},
      risk_factor_cholesky_{std::move(risk_factor_cholesky)} {

    if (risk_factor_names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (risk_factor_models_.intercepts.empty() || risk_factor_models_.coefficients.empty()) {
        throw core::HgpsException("Risk factor models mapping is incomplete");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
}

std::unique_ptr<HierarchicalLinearModel> StaticLinearModelDefinition::create_model() const {
    return std::make_unique<StaticLinearModel>(risk_factor_names_, risk_factor_models_,
                                               risk_factor_cholesky_);
}

} // namespace hgps
