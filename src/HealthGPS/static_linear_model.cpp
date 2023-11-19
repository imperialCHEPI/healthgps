#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

#include <ranges>

namespace hgps {

StaticLinearModel::StaticLinearModel(
    const RiskFactorSexAgeTable &expected, const std::vector<core::Identifier> &names,
    const std::vector<LinearModelParams> &models, const std::vector<double> &lambda,
    const std::vector<double> &stddev, const Eigen::MatrixXd &cholesky,
    const std::vector<LinearModelParams> &policy_models,
    const std::vector<core::DoubleInterval> &policy_ranges, const Eigen::MatrixXd &policy_cholesky,
    double info_speed,
    const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        &rural_prevalence,
    const std::unordered_map<core::Income, LinearModelParams> &income_models,
    double physical_activity_stddev)
    : RiskFactorAdjustableModel{expected}, names_{names}, models_{models}, lambda_{lambda},
      stddev_{stddev}, cholesky_{cholesky}, policy_models_{policy_models},
      policy_ranges_{policy_ranges}, policy_cholesky_{policy_cholesky}, info_speed_{info_speed},
      rural_prevalence_{rural_prevalence}, income_models_{income_models},
      physical_activity_stddev_{physical_activity_stddev} {

    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (lambda_.empty()) {
        throw core::HgpsException("Risk factor lambda list is empty");
    }
    if (stddev_.empty()) {
        throw core::HgpsException("Risk factor standard deviation list is empty");
    }
    if (!cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    if (policy_models_.empty()) {
        throw core::HgpsException("Intervention policy model list is empty");
    }
    if (policy_ranges_.empty()) {
        throw core::HgpsException("Intervention policy ranges list is empty");
    }
    if (!policy_cholesky_.allFinite()) {
        throw core::HgpsException("Intervention policy Cholesky matrix contains non-finite values");
    }
    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    if (income_models_.empty()) {
        throw core::HgpsException("Income models mapping is empty");
    }
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    // Initialise everyone.
    for (auto &person : context.population()) {
        initialise_sector(person, context.random());
        initialise_income(person, context.random());
        initialise_factors(person, context.random());
        initialise_physical_activity(person, context.random());
    }

    // Adjust risk factors to match expected values.
    adjust_risk_factors(context, names_);
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {

    // Initialise newborns and update others.
    for (auto &person : context.population()) {
        // Ignore if inactive.
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            initialise_sector(person, context.random());
            initialise_income(person, context.random());
            initialise_factors(person, context.random());
            initialise_physical_activity(person, context.random());
        } else {
            update_sector(person, context.random());
            update_income(person, context.random());
            update_factors(person, context.random());
        }
    }

    // Adjust risk factors to match expected values.
    adjust_risk_factors(context, names_);
}

void StaticLinearModel::initialise_factors(Person &person, Random &random) const {

    // Approximate risk factor values with linear models.
    auto linear_factors = compute_linear_models(person);

    // Correlated residual sampling.
    auto residuals = compute_residuals(random);

    // Initialise residuals and risk factors (do not exist yet).
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise residual.
        auto residual_name = core::Identifier{names_[i].to_string() + "_residual"};
        double residual_new = residuals[i];
        person.risk_factors[residual_name] = residual_new;

        // Initialise risk factor.
        double expected = get_risk_factor_expected().at(person.gender, names_[i]).at(person.age);
        double factor_new = linear_factors[i] + residual_new * stddev_[i];
        factor_new = expected * inverse_box_cox(factor_new, lambda_[i]);
        person.risk_factors[names_[i]] = factor_new;
    }
}

void StaticLinearModel::update_factors(Person &person, Random &random) const {

    // Approximate risk factor values with linear models.
    auto linear_factors = compute_linear_models(person);

    // Correlated residual sampling.
    auto residuals = compute_residuals(random);

    // Update residuals and risk factors (should exist).
    for (size_t i = 0; i < names_.size(); i++) {

        // Update residual.
        auto residual_name = core::Identifier{names_[i].to_string() + "_residual"};
        double residual_old = person.risk_factors.at(residual_name);
        double residual_new = residuals[i] * info_speed_;
        residual_new += sqrt(1.0 - info_speed_ * info_speed_) * residual_old;
        person.risk_factors.at(residual_name) = residual_new;

        // Update risk factor.
        double expected = get_risk_factor_expected().at(person.gender, names_[i]).at(person.age);
        double factor_new = linear_factors[i] + residual_new * stddev_[i];
        factor_new = expected * inverse_box_cox(factor_new, lambda_[i]);
        person.risk_factors.at(names_[i]) = factor_new;
    }
}

std::vector<double> StaticLinearModel::compute_linear_models(Person &person) const {
    std::vector<double> factors{};
    factors.reserve(names_.size());

    // Approximate risk factor values for person with linear models.
    for (size_t i = 0; i < names_.size(); i++) {
        auto name = names_[i];
        auto model = models_[i];
        double factor = model.intercept;
        for (const auto &[coefficient_name, coefficient_value] : model.coefficients) {
            factor += coefficient_value * person.get_risk_factor_value(coefficient_name);
        }
        factors.emplace_back(factor);
    }

    return factors;
}

std::vector<double> StaticLinearModel::compute_residuals(Random &random) const {
    std::vector<double> correlated_residuals{};
    correlated_residuals.reserve(names_.size());

    // Correlated samples using Cholesky decomposition.
    Eigen::VectorXd residuals{names_.size()};
    std::ranges::generate(residuals, [&random] { return random.next_normal(0.0, 1.0); });
    residuals = cholesky_ * residuals;

    // Save correlated residuals.
    for (size_t i = 0; i < names_.size(); i++) {
        correlated_residuals.emplace_back(residuals[i]);
    }

    return correlated_residuals;
}

double StaticLinearModel::inverse_box_cox(double factor, double lambda) {
    return pow(lambda * factor + 1.0, 1.0 / lambda);
}

void StaticLinearModel::initialise_sector(Person &person, Random &random) const {

    // Get rural prevalence for age group and sex.
    double prevalence;
    if (person.age < 18) {
        prevalence = rural_prevalence_.at("Under18"_id).at(person.gender);
    } else {
        prevalence = rural_prevalence_.at("Over18"_id).at(person.gender);
    }

    // Sample the person's sector.
    double rand = random.next_double();
    auto sector = rand < prevalence ? core::Sector::rural : core::Sector::urban;
    person.sector = sector;
}

void StaticLinearModel::update_sector(Person &person, Random &random) const {

    // Only update rural sector 18 year olds.
    if ((person.age != 18) || (person.sector != core::Sector::rural)) {
        return;
    }

    // Get rural prevalence for age group and sex.
    double prevalence_under18 = rural_prevalence_.at("Under18"_id).at(person.gender);
    double prevalence_over18 = rural_prevalence_.at("Over18"_id).at(person.gender);

    // Compute random rural to urban transition.
    double rand = random.next_double();
    double p_rural_to_urban = 1.0 - prevalence_over18 / prevalence_under18;
    if (rand < p_rural_to_urban) {
        person.sector = core::Sector::urban;
    }
}

void StaticLinearModel::initialise_income(Person &person, Random &random) const {

    // Compute logits for each income category.
    auto logits = std::unordered_map<core::Income, double>{};
    for (const auto &[income, params] : income_models_) {
        logits[income] = params.intercept;
        for (const auto &[factor, coefficient] : params.coefficients) {
            logits.at(income) += coefficient * person.get_risk_factor_value(factor);
        }
    }

    // Compute softmax probabilities for each income category.
    auto e_logits = std::unordered_map<core::Income, double>{};
    double e_logits_sum = 0.0;
    for (const auto &[income, logit] : logits) {
        e_logits[income] = exp(logit);
        e_logits_sum += e_logits.at(income);
    }

    // Compute income category probabilities.
    auto probabilities = std::unordered_map<core::Income, double>{};
    for (const auto &[income, e_logit] : e_logits) {
        probabilities[income] = e_logit / e_logits_sum;
    }

    // Compute income category.
    double rand = random.next_double();
    for (const auto &[income, probability] : probabilities) {
        if (rand < probability) {
            person.income = income;
            return;
        }
        rand -= probability;
    }

    throw core::HgpsException("Logic Error: failed to initialise income category");
}

void StaticLinearModel::update_income(Person &person, Random &random) const {

    // Only update 18 year olds.
    if (person.age == 18) {
        initialise_income(person, random);
    }
}

void StaticLinearModel::initialise_physical_activity(Person &person, Random &random) const {
    auto key = "PhysicalActivity"_id;
    double expected = get_risk_factor_expected().at(person.gender, key).at(person.age);
    double rand = random.next_normal(0.0, physical_activity_stddev_);
    double factor = expected * exp(rand - 0.5 * pow(physical_activity_stddev_, 2));
    person.risk_factors[key] = factor;
}

StaticLinearModelDefinition::StaticLinearModelDefinition(
    RiskFactorSexAgeTable expected, std::vector<core::Identifier> names,
    std::vector<LinearModelParams> models, std::vector<double> lambda, std::vector<double> stddev,
    Eigen::MatrixXd cholesky, std::vector<LinearModelParams> policy_models,
    std::vector<core::DoubleInterval> policy_ranges, Eigen::MatrixXd policy_cholesky,
    double info_speed,
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>> rural_prevalence,
    std::unordered_map<core::Income, LinearModelParams> income_models,
    double physical_activity_stddev)
    : RiskFactorAdjustableModelDefinition{std::move(expected)}, names_{std::move(names)},
      models_{std::move(models)}, lambda_{std::move(lambda)}, stddev_{std::move(stddev)},
      cholesky_{std::move(cholesky)}, policy_models_{std::move(policy_models)},
      policy_ranges_{std::move(policy_ranges)}, policy_cholesky_{std::move(policy_cholesky)},
      info_speed_{info_speed}, rural_prevalence_{std::move(rural_prevalence)},
      income_models_{std::move(income_models)},
      physical_activity_stddev_{physical_activity_stddev} {

    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (lambda_.empty()) {
        throw core::HgpsException("Risk factor lambda list is empty");
    }
    if (stddev_.empty()) {
        throw core::HgpsException("Risk factor standard deviation list is empty");
    }
    if (!cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    if (policy_models_.empty()) {
        throw core::HgpsException("Intervention policy model list is empty");
    }
    if (policy_ranges_.empty()) {
        throw core::HgpsException("Intervention policy ranges list is empty");
    }
    if (!policy_cholesky_.allFinite()) {
        throw core::HgpsException("Intervention policy Cholesky matrix contains non-finite values");
    }
    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    if (income_models_.empty()) {
        throw core::HgpsException("Income models mapping is empty");
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    const auto &expected = get_risk_factor_expected();
    return std::make_unique<StaticLinearModel>(expected, names_, models_, lambda_, stddev_,
                                               cholesky_, policy_models_, policy_ranges_,
                                               policy_cholesky_, info_speed_, rural_prevalence_,
                                               income_models_, physical_activity_stddev_);
}

} // namespace hgps
