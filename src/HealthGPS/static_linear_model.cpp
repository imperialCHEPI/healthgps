#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

#include <ranges>

namespace hgps {

StaticLinearModel::StaticLinearModel(
    const RiskFactorSexAgeTable &risk_factor_expected,
    const std::vector<LinearModelParams> &risk_factor_models,
    const Eigen::MatrixXd &risk_factor_cholesky,
    const std::map<core::Gender, std::vector<double>> &weight_quantiles =
        std::map<core::Gender, std::vector<double>>())
    : RiskFactorAdjustableModel{risk_factor_expected}, risk_factor_models_{risk_factor_models},
      risk_factor_cholesky_{risk_factor_cholesky}, weight_quantiles_{weight_quantiles} {

    if (risk_factor_models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    // TODO: Uncomment when fully implemented
    // if (weight_quantiles_.empty()) {
    //    throw core::HgpsException("Weigth quantiles dictionary is empty");
    //}
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    for (auto &person : context.population()) {

        // Approximate risk factor values with linear models.
        linear_approximation(person);

        // Initialise weight
        initialise_weight(person, context.random());

        // Correlated residual sampling.
        auto samples = correlated_samples(context);

        // TODO: add residuals to risk factor values
    }

    // Adjust weigth risk factor such its mean sim value matches expected value.
    adjust_risk_factors(context, {"Weight"_id});
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

/// Initialises the weight of a person.
///
/// It uses the baseline adjustment to get its initial value, based on its sex and age.
/// @param person The person fo initialise the weight for.
/// @generator Random number generator for the simulation.
void StaticLinearModel::initialise_weight(Person &person, Random &generator) {

    auto weight_bl = get_risk_factor_expected().at(person.gender, "Weight"_id).at(person.age);
    auto weight_quantile = get_weight_quantile(person.gender, generator);
    person.risk_factors["Weight"_id] = weight_bl * weight_quantile;
}

/// Returns the weight quantile for the given gender.
///
/// @gender The gender of the person.
/// @generator Random number generator for the simulation.
double StaticLinearModel::get_weight_quantile(core::Gender gender, Random &generator) {

    // TODO: Remove when fully implemented
    if (weight_quantiles_.empty()) {
        return 1.0;
    }

    auto index = static_cast<size_t>(generator.next_int(weight_quantiles_.at(gender).size()));
    return weight_quantiles_.at(gender)[index];
}

StaticLinearModelDefinition::StaticLinearModelDefinition(
    RiskFactorSexAgeTable risk_factor_expected, std::vector<LinearModelParams> risk_factor_models,
    Eigen::MatrixXd risk_factor_cholesky)
    : RiskFactorAdjustableModelDefinition{std::move(risk_factor_expected)},
      risk_factor_models_{std::move(risk_factor_models)},
      risk_factor_cholesky_{std::move(risk_factor_cholesky)} {

    if (risk_factor_models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    const auto &risk_factor_expected = get_risk_factor_expected();
    return std::make_unique<StaticLinearModel>(risk_factor_expected, risk_factor_models_,
                                               risk_factor_cholesky_);
}

} // namespace hgps
