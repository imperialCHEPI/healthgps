#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

#include <ranges>
#include <utility>

#include <iostream>
#include <oneapi/tbb/parallel_for_each.h>

namespace hgps {

StaticLinearModel::StaticLinearModel(
    std::shared_ptr<RiskFactorSexAgeTable> expected,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox,
    const std::vector<core::Identifier> &names, const std::vector<LinearModelParams> &models,
    const std::vector<core::DoubleInterval> &ranges, const std::vector<double> &lambda,
    const std::vector<double> &stddev, const Eigen::MatrixXd &cholesky,
    const std::vector<LinearModelParams> &policy_models,
    const std::vector<core::DoubleInterval> &policy_ranges, const Eigen::MatrixXd &policy_cholesky,
    std::shared_ptr<std::vector<LinearModelParams>> trend_models,
    std::shared_ptr<std::vector<core::DoubleInterval>> trend_ranges,
    std::shared_ptr<std::vector<double>> trend_lambda, double info_speed,
    const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        &rural_prevalence,
    const std::unordered_map<core::Income, LinearModelParams> &income_models,
    std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models,
    double physical_activity_stddev)
    : RiskFactorAdjustableModel{std::move(expected), std::move(expected_trend),
                                std::move(trend_steps)},
      expected_trend_boxcox_{std::move(expected_trend_boxcox)}, names_{names}, models_{models},
      ranges_{ranges}, lambda_{lambda}, stddev_{stddev}, cholesky_{cholesky},
      policy_models_{policy_models}, policy_ranges_{policy_ranges},
      policy_cholesky_{policy_cholesky}, trend_models_{std::move(trend_models)},
      trend_ranges_{std::move(trend_ranges)}, trend_lambda_{std::move(trend_lambda)},
      info_speed_{info_speed}, rural_prevalence_{rural_prevalence}, income_models_{income_models},
      region_models_{std::move(region_models)},
      physical_activity_stddev_{physical_activity_stddev} {}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    // Initialise everyone.
    // for (auto &person : context.population())
    auto &pop = context.population();
    tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        initialise_sector(person, context.random());
        initialise_region(person, context.random()); // added region for FINCH
        initialise_income(person, context.random());
        initialise_factors(context, person, context.random());
        initialise_physical_activity(context, person, context.random());
    });

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialise everyone.
    // for (auto &person : context.population())
    tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        initialise_policies(person, context.random(), false);
        initialise_trends(context, person);
    });

    // Adjust such that trended risk factor means match trended expected values.
    adjust_risk_factors(context, names_, ranges_, true);
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {

    // HACK: start intervening two years into the simulation.
    bool intervene = (context.scenario().type() == ScenarioType::intervention &&
                      (context.time_now() - context.start_time()) >= 2);

    // Initialise newborns and update others.
    // for (auto &person : context.population())
    auto &pop = context.population();
    tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }

        if (person.age == 0) {
            initialise_sector(person, context.random());
            initialise_region(person, context.random()); // added region for FINCH
            initialise_income(person, context.random());
            initialise_factors(context, person, context.random());
            initialise_physical_activity(context, person, context.random());
        } else {
            update_sector(person, context.random());
            update_income(person, context.random());
            update_factors(context, person, context.random());
        }
    });

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialise newborns and update others.
    // for (auto &person : context.population()) {
    tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }

        if (person.age == 0) {
            initialise_policies(person, context.random(), intervene);
            initialise_trends(context, person);
        } else {
            update_policies(person, intervene);
            update_trends(context, person);
        }
    });

    // Adjust such that trended risk factor means match trended expected values.
    adjust_risk_factors(context, names_, ranges_, true);

    // Apply policies if intervening.
    // for (auto &person : context.population()) {
    tbb::parallel_for_each(pop.begin(), pop.end(), [&](auto &person) {
        if (!person.is_active()) {
            return;
        }
        apply_policies(person, intervene);
    });
}

double StaticLinearModel::inverse_box_cox(double factor, double lambda) {
    return pow(lambda * factor + 1.0, 1.0 / lambda);
}

void StaticLinearModel::initialise_factors(RuntimeContext &context, Person &person,
                                           Random &random) const {

    // Correlated residual sampling.
    auto residuals = compute_residuals(random, cholesky_);

    // Approximate risk factors with linear models.
    auto linear = compute_linear_models(person, models_);

    // Initialise residuals and risk factors (do not exist yet).
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise residual.
        auto residual_name = core::Identifier{names_[i].to_string() + "_residual"};
        double residual = residuals[i];

        // Save residual.
        person.risk_factors[residual_name] = residual;

        // Initialise risk factor.
        double expected =
            get_expected(context, person.gender, person.age, names_[i], ranges_[i], false);
        double factor = linear[i] + residual * stddev_[i];
        factor = expected * inverse_box_cox(factor, lambda_[i]);
        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors[names_[i]] = factor;
    }
}

void StaticLinearModel::update_factors(RuntimeContext &context, Person &person,
                                       Random &random) const {

    // Correlated residual sampling.
    auto residuals = compute_residuals(random, cholesky_);

    // Approximate risk factors with linear models.
    auto linear = compute_linear_models(person, models_);

    // Update residuals and risk factors (should exist).
    for (size_t i = 0; i < names_.size(); i++) {

        // Update residual.
        auto residual_name = core::Identifier{names_[i].to_string() + "_residual"};
        double residual_old = person.risk_factors.at(residual_name);
        double residual = residuals[i] * info_speed_;
        residual += sqrt(1.0 - info_speed_ * info_speed_) * residual_old;

        // Save residual.
        person.risk_factors.at(residual_name) = residual;

        // Update risk factor.
        double expected =
            get_expected(context, person.gender, person.age, names_[i], ranges_[i], false);
        double factor = linear[i] + residual * stddev_[i];
        factor = expected * inverse_box_cox(factor, lambda_[i]);
        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors.at(names_[i]) = factor;
    }
}

void StaticLinearModel::initialise_trends(RuntimeContext &context, Person &person) const {

    // Approximate trends with linear models.
    auto linear = compute_linear_models(person, *trend_models_);

    // Initialise and apply trends (do not exist yet).
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_trend"};
        double expected = expected_trend_boxcox_->at(names_[i]);
        double trend = expected * inverse_box_cox(linear[i], (*trend_lambda_)[i]);
        trend = (*trend_ranges_)[i].clamp(trend);

        // Save trend.
        person.risk_factors[trend_name] = trend;
    }

    // Apply trends.
    update_trends(context, person);
}

void StaticLinearModel::update_trends(RuntimeContext &context, Person &person) const {

    // Get elapsed time (years).
    int elapsed_time = context.time_now() - context.start_time();

    // Apply trends (should exist).
    for (size_t i = 0; i < names_.size(); i++) {

        // Load trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_trend"};
        double trend = person.risk_factors.at(trend_name);

        // Apply trend to risk factor.
        double factor = person.risk_factors.at(names_[i]);
        int t = std::min(elapsed_time, get_trend_steps(names_[i]));
        factor *= pow(trend, t);
        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors.at(names_[i]) = factor;
    }
}

void StaticLinearModel::initialise_policies(Person &person, Random &random, bool intervene) const {
    // NOTE: we need to keep baseline and intervention scenario RNGs in sync,
    //       so we compute residuals even though they are not used in baseline.

    // Intervention policy residual components.
    auto residuals = compute_residuals(random, policy_cholesky_);

    // Save residuals (never updated in lifetime).
    for (size_t i = 0; i < names_.size(); i++) {
        auto residual_name = core::Identifier{names_[i].to_string() + "_policy_residual"};
        person.risk_factors[residual_name] = residuals[i];
    }

    // Compute policies.
    update_policies(person, intervene);
}

void StaticLinearModel::update_policies(Person &person, bool intervene) const {

    // Set zero policy if not intervening.
    if (!intervene) {
        for (const auto &name : names_) {
            auto policy_name = core::Identifier{name.to_string() + "_policy"};
            person.risk_factors[policy_name] = 0.0;
        }
        return;
    }

    // Intervention policy linear components.
    auto linear = compute_linear_models(person, policy_models_);

    // Compute all intervention policies.
    for (size_t i = 0; i < names_.size(); i++) {

        // Load residual component.
        auto residual_name = core::Identifier{names_[i].to_string() + "_policy_residual"};
        double residual = person.risk_factors.at(residual_name);

        // Compute policy.
        auto policy_name = core::Identifier{names_[i].to_string() + "_policy"};
        double policy = linear[i] + residual;
        policy = policy_ranges_[i].clamp(policy);

        // Save policy.
        person.risk_factors[policy_name] = policy;
    }
}

void StaticLinearModel::apply_policies(Person &person, bool intervene) const {

    // No-op if not intervening.
    if (!intervene) {
        return;
    }

    // Apply all intervention policies.
    for (size_t i = 0; i < names_.size(); i++) {

        // Load policy.
        auto policy_name = core::Identifier{names_[i].to_string() + "_policy"};
        double policy = person.risk_factors.at(policy_name);

        // Apply policy to risk factor.
        double factor_old = person.risk_factors.at(names_[i]);
        double factor = factor_old * (1.0 + policy / 100.0);
        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors.at(names_[i]) = factor;
    }
}

std::vector<double>
StaticLinearModel::compute_linear_models(Person &person,
                                         const std::vector<LinearModelParams> &models) const {
    std::vector<double> linear{};
    linear.reserve(names_.size());

    // Approximate risk factors with linear models.
    for (size_t i = 0; i < names_.size(); i++) {
        auto name = names_[i];
        auto model = models[i];
        double factor = model.intercept;
        for (const auto &[coefficient_name, coefficient_value] : model.coefficients) {
            factor += coefficient_value * person.get_risk_factor_value(coefficient_name);
        }
        for (const auto &[coefficient_name, coefficient_value] : model.log_coefficients) {
            factor += coefficient_value * log(person.get_risk_factor_value(coefficient_name));
        }
        linear.emplace_back(factor);
    }

    return linear;
}

std::vector<double> StaticLinearModel::compute_residuals(Random &random,
                                                         const Eigen::MatrixXd &cholesky) const {
    std::vector<double> correlated_residuals{};
    correlated_residuals.reserve(names_.size());

    // Correlated samples using Cholesky decomposition.
    Eigen::VectorXd residuals{names_.size()};
    std::ranges::generate(residuals, [&random] { return random.next_normal(0.0, 1.0); });
    residuals = cholesky * residuals;

    // Save correlated residuals.
    for (size_t i = 0; i < names_.size(); i++) {
        correlated_residuals.emplace_back(residuals[i]);
    }

    return correlated_residuals;
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

void StaticLinearModel::initialise_physical_activity(RuntimeContext &context, Person &person,
                                                     Random &random) const {
    double expected = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                   std::nullopt, false);
    double rand = random.next_normal(0.0, physical_activity_stddev_);
    double factor = expected * exp(rand - 0.5 * pow(physical_activity_stddev_, 2));
    person.risk_factors["PhysicalActivity"_id] = factor;
}

void StaticLinearModel::initialise_region(Person &person, Random &random) const {
    // Compute logits for each region category
    auto logits = std::unordered_map<core::Region, double>{};
    for (const auto &[region, params] : *region_models_) {
        logits[region] = params.intercept;
        for (const auto &[factor, coefficient] : params.coefficients) {
            logits.at(region) += coefficient * person.get_risk_factor_value(factor);
        }
    }

    // Compute softmax probabilities for each region
    auto e_logits = std::unordered_map<core::Region, double>{};
    double e_logits_sum = 0.0;
    for (const auto &[region, logit] : logits) {
        e_logits[region] = exp(logit);
        e_logits_sum += e_logits.at(region);
    }

    // Compute region probabilities for each region
    auto probabilities = std::unordered_map<core::Region, double>{};
    for (const auto &[region, e_logit] : e_logits) {
        probabilities[region] = e_logit / e_logits_sum;
    }

    // Sample region need to do this for each person (TO VERIFY)
    double rand = random.next_double();
    for (const auto &[region, probability] : probabilities) {
        if (rand < probability) {
            person.region = region;
            return;
        }
        rand -= probability;
    }

    throw core::HgpsException("Logic Error: failed to initialise region category");
}

void StaticLinearModel::update_region(Person &person, Random &random) const {
    // Only update 18 year olds
    if (person.age == 18) {
        initialise_region(person, random);
    }
}

StaticLinearModelDefinition::StaticLinearModelDefinition( 
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox,
    std::vector<core::Identifier> names, std::vector<LinearModelParams> models,
    std::vector<core::DoubleInterval> ranges, std::vector<double> lambda,
    std::vector<double> stddev, Eigen::MatrixXd cholesky,
    std::vector<LinearModelParams> policy_models, std::vector<core::DoubleInterval> policy_ranges,
    Eigen::MatrixXd policy_cholesky, std::unique_ptr<std::vector<LinearModelParams>> trend_models,
    std::unique_ptr<std::vector<core::DoubleInterval>> trend_ranges,
    std::unique_ptr<std::vector<double>> trend_lambda, double info_speed,
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>> rural_prevalence,
    std::unordered_map<core::Income, LinearModelParams> income_models,
    std::unordered_map<core::Region, LinearModelParams> region_models,
    double physical_activity_stddev)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps)},
      expected_trend_boxcox_{std::move(expected_trend_boxcox)}, names_{std::move(names)},
      models_{std::move(models)}, ranges_{std::move(ranges)}, lambda_{std::move(lambda)},
      stddev_{std::move(stddev)}, cholesky_{std::move(cholesky)},
      policy_models_{std::move(policy_models)}, policy_ranges_{std::move(policy_ranges)},
      policy_cholesky_{std::move(policy_cholesky)}, trend_models_{std::move(trend_models)},
      trend_ranges_{std::move(trend_ranges)}, trend_lambda_{std::move(trend_lambda)},
      info_speed_{info_speed}, rural_prevalence_{std::move(rural_prevalence)},
      income_models_{std::move(income_models)}, region_models_{std::move(region_models)},
      physical_activity_stddev_{physical_activity_stddev} {

    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (ranges_.empty()) {
        throw core::HgpsException("Risk factor ranges list is empty");
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
    if (trend_models_->empty()) {
        throw core::HgpsException("Time trend model list is empty");
    }
    if (trend_ranges_->empty()) {
        throw core::HgpsException("Time trend ranges list is empty");
    }
    if (trend_lambda_->empty()) {
        throw core::HgpsException("Time trend lambda list is empty");
    }
    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    if (income_models_.empty()) {
        throw core::HgpsException("Income models mapping is empty");
    }
    if (region_models_.empty()) {
        throw core::HgpsException("Region models mapping is empty");
    }
    for (const auto &name : names_) {
        if (!expected_trend_->contains(name)) {
            throw core::HgpsException("One or more expected trend value is missing");
        }
        if (!expected_trend_boxcox_->contains(name)) {
            throw core::HgpsException("One or more expected trend BoxCox value is missing");
        }
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    return std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_, names_, models_, ranges_,
        lambda_, stddev_, cholesky_, policy_models_, policy_ranges_, policy_cholesky_,
        trend_models_, trend_ranges_, trend_lambda_, info_speed_, rural_prevalence_, income_models_,
        std::make_shared<std::unordered_map<core::Region, LinearModelParams>>(region_models_),
        physical_activity_stddev_);
}

} // namespace hgps
