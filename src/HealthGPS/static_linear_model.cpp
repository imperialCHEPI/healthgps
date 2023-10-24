#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

#include <ranges>

namespace hgps {

StaticLinearModel::StaticLinearModel(std::vector<LinearModelParams> risk_factor_models,
                                     Eigen::MatrixXd risk_factor_cholesky)
    : risk_factor_models_{std::move(risk_factor_models)},
      risk_factor_cholesky_{std::move(risk_factor_cholesky)} {

    if (risk_factor_models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

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
    Eigen::VectorXd samples{risk_factor_models_.size()};
    std::ranges::generate(samples, [&context] { return context.random().next_normal(0.0, 1.0); });
    samples = risk_factor_cholesky_ * samples;

    // Bound and scale the samples.
    std::ranges::transform(samples, samples.begin(), [](double sample) {
        constexpr double sigma_cap = 3.0; // for removing outliers
        return std::clamp(sample, -sigma_cap, sigma_cap) / sigma_cap;
    });

    return samples;
}

void StaticLinearModel::linear_approximation(Person &person) {

    // Approximate risk factor values for person with linear models.
    for (const auto &model : risk_factor_models_) {
        double factor = model.intercept;
        for (const auto &[coefficient_name, coefficient_value] : model.coefficients) {
            factor += coefficient_value * person.get_risk_factor_value(coefficient_name);
        }
        person.risk_factors[model.name] = factor;
    }
}

StaticLinearModelDefinition::StaticLinearModelDefinition(
    std::vector<LinearModelParams> risk_factor_models, Eigen::MatrixXd risk_factor_cholesky)
    : risk_factor_models_{std::move(risk_factor_models)},
      risk_factor_cholesky_{std::move(risk_factor_cholesky)} {

    if (risk_factor_models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    return std::make_unique<StaticLinearModel>(risk_factor_models_, risk_factor_cholesky_);
}

} // namespace hgps
