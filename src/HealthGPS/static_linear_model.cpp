#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

#include <ranges>

namespace hgps {

StaticLinearModel::StaticLinearModel(
    const RiskFactorSexAgeTable &risk_factor_expected,
    const std::vector<LinearModelParams> &risk_factor_models,
    const Eigen::MatrixXd &risk_factor_cholesky,
    const std::unordered_map<hgps::core::Identifier, std::unordered_map<hgps::core::Gender, double>>
        &rural_prevalence,
    const std::vector<LinearModelParams> &income_models)
    : RiskFactorAdjustableModel{risk_factor_expected}, risk_factor_models_{risk_factor_models},
      risk_factor_cholesky_{risk_factor_cholesky}, rural_prevalence_{rural_prevalence},
      income_models_{income_models} {

    if (risk_factor_models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    if (income_models_.empty()) {
        throw core::HgpsException("Income models list is empty");
    }
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    // Initialise income and sector for everyone.
    for (auto &person : context.population()) {
        initialise_sector(context, person);
        initialise_income(context, person);
    }

    // Initialise risk factors for everyone.
    for (auto &person : context.population()) {

        // Approximate risk factor values with linear models.
        linear_approximation(person);

        // Correlated residual sampling.
        auto samples = correlated_sample(context);

        // TODO: add residuals to risk factor values
        // TODO: separate functions for init and update, like in sector/income
    }
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {

    // Initialise sector and income for newborns and update others.
    for (auto &person : context.population()) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            initialise_sector(context, person);
            initialise_income(context, person);
        } else {
            update_sector(context, person);
            update_income(context, person);
        }
    }

    // Initialise risk factors for newborns and update others.
    for (auto &person : context.population()) {

        // Do not initialise non-newborns.
        if (!person.is_active() || person.age > 0) {
            continue;
        }

        // Approximate risk factor values with linear models.
        linear_approximation(person);

        // Correlated residual sampling.
        auto samples = correlated_sample(context);

        // TODO: add residuals to risk factor values
        // TODO: separate functions for init and update, like in sector/income
    }
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

Eigen::VectorXd StaticLinearModel::correlated_sample(RuntimeContext &context) {

    // Correlated samples using Cholesky decomposition.
    Eigen::VectorXd samples{risk_factor_models_.size()};
    std::ranges::generate(samples, [&context] { return context.random().next_normal(0.0, 1.0); });
    return risk_factor_cholesky_ * samples;
}

void StaticLinearModel::initialise_sector(RuntimeContext &context, Person &person) const {

    // Get rural prevalence for age group and sex.
    double prevalence;
    if (person.age < 18) {
        prevalence = rural_prevalence_.at("Under18"_id).at(person.gender);
    } else {
        prevalence = rural_prevalence_.at("Over18"_id).at(person.gender);
    }

    // Sample the person's sector.
    double rand = context.random().next_double();
    auto sector = rand < prevalence ? core::Sector::rural : core::Sector::urban;
    person.sector = sector;
}

void StaticLinearModel::update_sector(RuntimeContext &context, Person &person) const {

    // Only update rural sector 18 year olds.
    if ((person.age != 18) || (person.sector != core::Sector::rural)) {
        return;
    }

    // Get rural prevalence for age group and sex.
    double prevalence_under18 = rural_prevalence_.at("Under18"_id).at(person.gender);
    double prevalence_over18 = rural_prevalence_.at("Over18"_id).at(person.gender);

    // Compute random rural to urban transition.
    double rand = context.random().next_double();
    double p_rural_to_urban = 1.0 - prevalence_over18 / prevalence_under18;
    if (rand < p_rural_to_urban) {
        person.sector = core::Sector::urban;
    }
}

void StaticLinearModel::initialise_income(RuntimeContext &context, Person &person) const {

    // Compute logits for each income category.
    auto logits = std::vector<double>{};
    logits.reserve(income_models_.size());
    for (const auto &income_model : income_models_) {
        logits.push_back(income_model.intercept);
        for (const auto &[factor_name, coefficient] : income_model.coefficients) {
            logits.back() += coefficient * person.get_risk_factor_value(factor_name);
        }
    }

    // Compute softmax probabilities for each income category.
    auto e_logits = std::vector<double>{};
    e_logits.reserve(income_models_.size());
    double e_logits_sum = 0.0;
    for (const auto &logit : logits) {
        e_logits.push_back(exp(logit));
        e_logits_sum += e_logits.back();
    }

    // Compute income category probabilities.
    auto probabilities = std::vector<double>{};
    probabilities.reserve(income_models_.size());
    for (const auto &e_logit : e_logits) {
        probabilities.push_back(e_logit / e_logits_sum);
    }

    // Compute income category.
    double rand = context.random().next_double();
    for (size_t i = 0; i < income_models_.size(); i++) {
        if (rand < probabilities[i]) {
            person.income = income_models_[i].name;
            return;
        }
        rand -= probabilities[i];
    }

    throw core::HgpsException("Logic Error: failed to initialise income category");
}

void StaticLinearModel::update_income(RuntimeContext &context, Person &person) const {

    // Only update 18 year olds.
    if (person.age == 18) {
        initialise_income(context, person);
    }
}

StaticLinearModelDefinition::StaticLinearModelDefinition(
    RiskFactorSexAgeTable risk_factor_expected, std::vector<LinearModelParams> risk_factor_models,
    Eigen::MatrixXd risk_factor_cholesky,
    std::unordered_map<hgps::core::Identifier, std::unordered_map<hgps::core::Gender, double>>
        rural_prevalence,
    std::vector<LinearModelParams> income_models)
    : RiskFactorAdjustableModelDefinition{std::move(risk_factor_expected)},
      risk_factor_models_{std::move(risk_factor_models)},
      risk_factor_cholesky_{std::move(risk_factor_cholesky)},
      rural_prevalence_{std::move(rural_prevalence)}, income_models_{std::move(income_models)} {

    if (risk_factor_models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    if (income_models_.empty()) {
        throw core::HgpsException("Income models list is empty");
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    const auto &risk_factor_expected = get_risk_factor_expected();
    return std::make_unique<StaticLinearModel>(risk_factor_expected, risk_factor_models_,
                                               risk_factor_cholesky_, rural_prevalence_,
                                               income_models_);
}

} // namespace hgps
