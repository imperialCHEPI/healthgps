#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

#include <cmath>
#include <fstream>  // MAHIMA: For file operations
#include <iomanip>  // MAHIMA: For precision formatting
#include <iostream> // Added for print statements
#include <ranges>
#include <sstream> // MAHIMA: For string streams
#include <utility>

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
    double physical_activity_stddev, TrendType trend_type,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox,
    std::shared_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps,
    std::shared_ptr<std::vector<LinearModelParams>> income_trend_models,
    std::shared_ptr<std::vector<core::DoubleInterval>> income_trend_ranges,
    std::shared_ptr<std::vector<double>> income_trend_lambda,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors)
    : RiskFactorAdjustableModel{std::move(expected),       std::move(expected_trend),
                                std::move(trend_steps),    trend_type,
                                expected_income_trend,       // Pass by value, not moved
                                income_trend_decay_factors}, // Pass by value, not moved
      // Regular trend member variables
      expected_trend_boxcox_{std::move(expected_trend_boxcox)},
      trend_models_{std::move(trend_models)}, trend_ranges_{std::move(trend_ranges)},
      trend_lambda_{std::move(trend_lambda)},
      // Income trend member variables
      trend_type_{trend_type}, expected_income_trend_{std::move(expected_income_trend)},
      expected_income_trend_boxcox_{std::move(expected_income_trend_boxcox)},
      income_trend_steps_{std::move(income_trend_steps)},
      income_trend_models_{std::move(income_trend_models)},
      income_trend_ranges_{std::move(income_trend_ranges)},
      income_trend_lambda_{std::move(income_trend_lambda)},
      income_trend_decay_factors_{std::move(income_trend_decay_factors)},
      // Common member variables
      names_{names}, models_{models}, ranges_{ranges}, lambda_{lambda}, stddev_{stddev},
      cholesky_{cholesky}, policy_models_{policy_models}, policy_ranges_{policy_ranges},
      policy_cholesky_{policy_cholesky}, info_speed_{info_speed},
      rural_prevalence_{rural_prevalence}, income_models_{income_models},
      physical_activity_stddev_{physical_activity_stddev} {

    // ===== MAHIMA: Initialize risk factor inspection settings =====
    initialize_inspection_settings();
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {
    // Initialise everyone.
    for (auto &person : context.population()) {
        initialise_sector(person, context.random());
        initialise_income(person, context.random());
        initialise_factors(context, person, context.random());
        initialise_physical_activity(context, person, context.random());
    }

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialise everyone with appropriate trend type.
    for (auto &person : context.population()) {
        initialise_policies(person, context.random(), false);

        // Apply trend based on trend_type
        switch (trend_type_) {
        case TrendType::Null:
            // No trends applied
            break;
        case TrendType::Trend:
            initialise_trends(context, person);
            break;
        case TrendType::IncomeTrend:
            initialise_income_trends(context, person);
            break;
        }
    }

    // Adjust such that trended risk factor means match trended expected values.
    // Only apply trend adjustment if we have a trend type
    if (trend_type_ != TrendType::Null) {
        adjust_risk_factors(context, names_, ranges_, true);
    }

    // ===== MAHIMA: Write inspection data if any was collected =====
    // Note: Physical activity is assigned after risk factors, so we write here
    write_inspection_data(context);
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {
    // HACK: start intervening two years into the simulation.
    bool intervene = (context.scenario().type() == ScenarioType::intervention &&
                      (context.time_now() - context.start_time()) >= 2);

    // Initialise newborns and update others.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            initialise_sector(person, context.random());
            initialise_income(person, context.random());
            initialise_factors(context, person, context.random());
            initialise_physical_activity(context, person, context.random());
        } else {
            update_sector(person, context.random());
            update_income(person, context.random());
            update_factors(context, person, context.random());
        }
    }

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialise newborns and update others with appropriate trend type.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            initialise_policies(person, context.random(), intervene);

            // Apply trend based on trend_type
            switch (trend_type_) {
            case TrendType::Null:
                // No trends applied
                break;
            case TrendType::Trend:
                initialise_trends(context, person);
                break;
            case TrendType::IncomeTrend:
                initialise_income_trends(context, person);
                break;
            }
        } else {
            update_policies(person, intervene);

            // Apply trend based on trend_type
            switch (trend_type_) {
            case TrendType::Null:
                // No trends applied
                break;
            case TrendType::Trend:
                update_trends(context, person);
                break;
            case TrendType::IncomeTrend:
                update_income_trends(context, person);
                break;
            }
        }
    }

    // Adjust such that trended risk factor means match trended expected values.
    // Only apply trend adjustment if we have a trend type
    if (trend_type_ != TrendType::Null) {
        adjust_risk_factors(context, names_, ranges_, true);
    }

    // Apply policies if intervening.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        apply_policies(person, intervene);
    }

    // ===== MAHIMA: Write inspection data if any was collected (for newborns only) =====
    // Note: Physical activity is assigned after risk factors, so we write here
    write_inspection_data(context);
}

double StaticLinearModel::inverse_box_cox(double factor, double lambda) {
    double base = (lambda * factor) + 1.0;
    double result = pow(base, 1.0 / lambda);
    return result;
}

void StaticLinearModel::initialise_factors(RuntimeContext &context, Person &person,
                                           Random &random) const {

    // Correlated residual sampling.
    auto residuals = compute_residuals(random, cholesky_);

    // Approximate risk factors with linear models.
    auto linear = compute_linear_models(person, models_);

    // Initialise residuals and risk factors (do not exist yet).
    for (size_t i = 0; i < names_.size(); i++) {
        const auto &factor_name = names_[i];

        // ===== MAHIMA: Check if we should inspect this factor for this person =====
        bool should_inspect_this = should_inspect(person, factor_name, context);

        // Initialise residual.
        auto residual_name = core::Identifier{factor_name.to_string() + "_residual"};
        double residual = residuals[i];

        // ===== MAHIMA: Record residual initialization if inspection is enabled =====
        if (should_inspect_this) {
            record_inspection_data(person, factor_name, context, "residual_initialization",
                                   residual, 0.0, 0.0, residual, stddev_[i], lambda_[i], 0.0, 0.0,
                                   ranges_[i].lower(), ranges_[i].upper(), 0.0, residuals[i],
                                   residual);
        }

        // Save residual.
        person.risk_factors[residual_name] = residual;

        // Initialise risk factor.
        double expected =
            get_expected(context, person.gender, person.age, factor_name, ranges_[i], false);
        double linear_result = linear[i];
        double factor_before_boxcox = linear_result + residual * stddev_[i];
        double boxcox_result = inverse_box_cox(factor_before_boxcox, lambda_[i]);
        double factor_before_clamp = expected * boxcox_result;
        double final_factor = ranges_[i].clamp(factor_before_clamp);

        // ===== MAHIMA: Record factor initialization if inspection is enabled =====
        if (should_inspect_this) {
            record_inspection_data(
                person, factor_name, context, "factor_initialization", final_factor, expected,
                linear_result, residual, stddev_[i], lambda_[i], boxcox_result, factor_before_clamp,
                ranges_[i].lower(), ranges_[i].upper(), final_factor, residuals[i], residual);
        }

        // Save risk factor.
        person.risk_factors[factor_name] = final_factor;
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

// This function is for intialising UPF Trends
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

// This function is for updating UPF Trends
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

// This function is for intialising Income Trends
void StaticLinearModel::initialise_income_trends(RuntimeContext &context, Person &person) const {
    // Check if income trend data is available
    if (!income_trend_models_ || !expected_income_trend_boxcox_ || !income_trend_lambda_ ||
        !income_trend_ranges_) {
        // If income trend data is not available, skip initialization
        std::cout << "Income trend data is not available, skipping initialization";
        return;
    }

    // Approximate income trends with linear models.
    auto linear = compute_linear_models(person, *income_trend_models_);

    // Initialise and apply income trends
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise income trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_income_trend"};
        double expected = expected_income_trend_boxcox_->at(names_[i]);

        double trend = expected * inverse_box_cox(linear[i], (*income_trend_lambda_)[i]);

        trend = (*income_trend_ranges_)[i].clamp(trend);

        // Save income trend.
        person.risk_factors[trend_name] = trend;
    }

    // Apply income trends.
    update_income_trends(context, person);
}

// This function is for updating Income Trends
void StaticLinearModel::update_income_trends(RuntimeContext &context, Person &person) const {
    // Get elapsed time (years). This is the income_trend_steps.
    int elapsed_time = context.time_now() - context.start_time();

    // Apply income trends
    for (size_t i = 0; i < names_.size(); i++) {

        // Load income trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_income_trend"};
        double trend = person.risk_factors.at(trend_name);

        // Apply income trend to risk factor.
        double factor = person.risk_factors.at(names_[i]);

        // Income trend is applied from the second year (T > T0)
        if (elapsed_time > 0) {

            // Check if income trend data is available
            if (income_trend_decay_factors_ && income_trend_steps_) {
                // Get the decay factor for this risk factor
                double decay_factor = income_trend_decay_factors_->at(names_[i]);

                // Cap the trend application at income_trend_steps
                int t = std::min(elapsed_time, income_trend_steps_->at(names_[i]));

                // Calculate income trend: trend_income_T = trend_income_T0 * e^(b*(T-T0))
                double exponent = decay_factor * t;
                double trend_income_T = trend * exp(exponent);
                factor *= trend_income_T;
            }
            // If income trend data is not available, skip income trend application
        } else {
            // Skipping income trend (elapsed_time <= 0)
        }

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
    double physical_activity_stddev, TrendType trend_type,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps,
    std::unique_ptr<std::vector<LinearModelParams>> income_trend_models,
    std::unique_ptr<std::vector<core::DoubleInterval>> income_trend_ranges,
    std::unique_ptr<std::vector<double>> income_trend_lambda,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps)},
      // Regular trend member variables
      expected_trend_boxcox_{std::move(expected_trend_boxcox)},
      trend_models_{std::move(trend_models)}, trend_ranges_{std::move(trend_ranges)},
      trend_lambda_{std::move(trend_lambda)},
      // Income trend member variables
      trend_type_{trend_type}, expected_income_trend_{std::move(expected_income_trend)},
      expected_income_trend_boxcox_{std::move(expected_income_trend_boxcox)},
      income_trend_steps_{std::move(income_trend_steps)},
      income_trend_models_{std::move(income_trend_models)},
      income_trend_ranges_{std::move(income_trend_ranges)},
      income_trend_lambda_{std::move(income_trend_lambda)},
      income_trend_decay_factors_{std::move(income_trend_decay_factors)},
      // Common member variables
      names_{std::move(names)}, models_{std::move(models)}, ranges_{std::move(ranges)},
      lambda_{std::move(lambda)}, stddev_{std::move(stddev)}, cholesky_{std::move(cholesky)},
      policy_models_{std::move(policy_models)}, policy_ranges_{std::move(policy_ranges)},
      policy_cholesky_{std::move(policy_cholesky)}, info_speed_{info_speed},
      rural_prevalence_{std::move(rural_prevalence)}, income_models_{std::move(income_models)},
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
    // Validate regular trend parameters only if trend type is Trend
    if (trend_type_ == TrendType::Trend) {
        if (trend_models_->empty()) {
            throw core::HgpsException("Time trend model list is empty");
        }
        if (trend_ranges_->empty()) {
            throw core::HgpsException("Time trend ranges list is empty");
        }
        if (trend_lambda_->empty()) {
            throw core::HgpsException("Time trend lambda list is empty");
        }
    }

    // Validate income trend parameters if income trend is enabled
    if (trend_type_ == TrendType::IncomeTrend) {
        if (!expected_income_trend_) {
            throw core::HgpsException(
                "Income trend is enabled but expected_income_trend is missing");
        }
        if (!expected_income_trend_boxcox_) {
            throw core::HgpsException(
                "Income trend is enabled but expected_income_trend_boxcox is missing");
        }
        if (!income_trend_steps_) {
            throw core::HgpsException("Income trend is enabled but income_trend_steps is missing");
        }
        if (!income_trend_models_) {
            throw core::HgpsException("Income trend is enabled but income_trend_models is missing");
        }
        if (!income_trend_ranges_) {
            throw core::HgpsException("Income trend is enabled but income_trend_ranges is missing");
        }
        if (!income_trend_lambda_) {
            throw core::HgpsException("Income trend is enabled but income_trend_lambda is missing");
        }
        if (!income_trend_decay_factors_) {
            throw core::HgpsException(
                "Income trend is enabled but income_trend_decay_factors is missing");
        }

        // Validate income trend data consistency
        if (income_trend_models_->empty()) {
            throw core::HgpsException("Income trend model list is empty");
        }
        if (income_trend_ranges_->empty()) {
            throw core::HgpsException("Income trend ranges list is empty");
        }
        if (income_trend_lambda_->empty()) {
            throw core::HgpsException("Income trend lambda list is empty");
        }

        // Validate that all risk factors have income trend parameters
        for (const auto &name : names_) {
            if (!expected_income_trend_->contains(name)) {
                throw core::HgpsException(
                    "One or more expected income trend value is missing for risk factor: " +
                    name.to_string());
            }
            if (!expected_income_trend_boxcox_->contains(name)) {
                throw core::HgpsException(
                    "One or more expected income trend BoxCox value is missing for risk factor: " +
                    name.to_string());
            }
            if (!income_trend_steps_->contains(name)) {
                throw core::HgpsException(
                    "One or more income trend steps value is missing for risk factor: " +
                    name.to_string());
            }
            if (!income_trend_decay_factors_->contains(name)) {
                throw core::HgpsException(
                    "One or more income trend decay factor is missing for risk factor: " +
                    name.to_string());
            }
        }
    }

    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    if (income_models_.empty()) {
        throw core::HgpsException("Income models mapping is empty");
    }

    // Validate regular trend parameters for all risk factors only if trend type is Trend
    if (trend_type_ == TrendType::Trend) {
        for (const auto &name : names_) {
            if (!expected_trend_->contains(name)) {
                throw core::HgpsException("One or more expected trend value is missing");
            }
            if (!expected_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected trend BoxCox value is missing");
            }
        }
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    return std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_, names_, models_, ranges_,
        lambda_, stddev_, cholesky_, policy_models_, policy_ranges_, policy_cholesky_,
        trend_models_, trend_ranges_, trend_lambda_, info_speed_, rural_prevalence_, income_models_,
        physical_activity_stddev_, trend_type_, expected_income_trend_,
        expected_income_trend_boxcox_, income_trend_steps_, income_trend_models_,
        income_trend_ranges_, income_trend_lambda_, income_trend_decay_factors_);
}

// ===== MAHIMA: Risk Factor Inspection Implementation =====

void StaticLinearModel::initialize_inspection_settings() {
    // Set to true to enable inspection, false to disable
    inspection_settings_.enabled = true; // CHANGE THIS TO TRUE TO ENABLE

    // Configure which risk factor to inspect
    inspection_settings_.target_risk_factor = "FoodFat"_id; // CHANGE THIS TO DESIRED FACTOR

    // Optional filters - set to std::nullopt to disable filtering
    inspection_settings_.target_age = 30; // e.g., 30 for age 30 only
    inspection_settings_.target_gender =
        core::Gender::female;                // e.g., core::Gender::male, for males only
    inspection_settings_.target_year = 2022; // e.g., 2025 for year 2025 only

    // Initialize inspection data if enabled
    if (inspection_settings_.enabled) {
        inspection_data_ = std::make_unique<std::vector<std::string>>();
        std::cout << "\nRisk factor inspection enabled for: "
                  << inspection_settings_.target_risk_factor.to_string();
    }
}

bool StaticLinearModel::should_inspect(const Person &person, const core::Identifier &factor_name,
                                       const RuntimeContext &context) const {
    if (!inspection_settings_.enabled) {
        return false;
    }

    // Check if this is the target risk factor
    if (factor_name != inspection_settings_.target_risk_factor) {
        return false;
    }

    // Check age filter
    if (inspection_settings_.target_age.has_value() &&
        static_cast<int>(person.age) != inspection_settings_.target_age.value()) {
        return false;
    }

    // Check gender filter
    if (inspection_settings_.target_gender.has_value() &&
        person.gender != inspection_settings_.target_gender.value()) {
        return false;
    }

    // Check year filter
    if (inspection_settings_.target_year.has_value() &&
        context.time_now() != inspection_settings_.target_year.value()) {
        return false;
    }

    return true;
}

void StaticLinearModel::record_inspection_data(
    const Person &person, const core::Identifier &factor_name, const RuntimeContext &context,
    const std::string &step_name, double value_assigned, double expected_value,
    double linear_result, double residual, double stddev, double lambda, double boxcox_result,
    double factor_before_clamp, double range_lower, double range_upper, double final_clamped_factor,
    double random_residual_before_cholesky, double residual_after_cholesky) const {
    (void)context;     // Suppress unused parameter warning
    (void)factor_name; // Suppress unused parameter warning

    if (!inspection_data_) {
        return;
    }

    // Get physical activity value if available, otherwise use 0.0
    double physical_activity = 0.0;
    auto it = person.risk_factors.find("PhysicalActivity"_id);
    if (it != person.risk_factors.end()) {
        physical_activity = it->second;
    }

    std::string csv_line = create_inspection_csv_line(
        person.id(), person.gender, person.age, person.sector, person.income, step_name, 
        value_assigned, expected_value, linear_result, residual, stddev, lambda, boxcox_result, 
        factor_before_clamp, range_lower, range_upper, final_clamped_factor,
        random_residual_before_cholesky, residual_after_cholesky, physical_activity);

    inspection_data_->emplace_back(std::move(csv_line));
}

void StaticLinearModel::write_inspection_data(
    [[maybe_unused]] const RuntimeContext &context) const {
    (void)context; // Suppress unused parameter warning

    if (!inspection_data_ || inspection_data_->empty()) {
        return;
    }

    // Generate filename with risk factor name
    std::string factor_name = inspection_settings_.target_risk_factor.to_string();
    std::string filename = factor_name + "_inspection.csv";

    // Write to CSV file
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "\nCannot open file for writing: " << filename;
        return;
    }

    // Write header
    file << get_inspection_csv_header() << "\n";

    // Write records
    for (const auto &record : *inspection_data_) {
        file << record << "\n";
    }

    std::cout << "\nRisk factor inspection data written to: " << filename << " ("
              << inspection_data_->size() << " records)";
}

std::string StaticLinearModel::create_inspection_csv_line(
    std::size_t person_id, core::Gender gender, unsigned int age, core::Sector sector,
    core::Income income_category, const std::string &step_name,
    double value_assigned, double expected_value, double linear_result, double residual,
    double stddev, double lambda, double boxcox_result, double factor_before_clamp,
    double range_lower, double range_upper, double final_clamped_factor,
    double random_residual_before_cholesky, double residual_after_cholesky,
    double physical_activity) const {
    std::ostringstream oss;
    oss << person_id << "," << static_cast<int>(gender) << "," << age << ","
        << static_cast<int>(sector) << ","                               // sector (rural/urban)
        << std::fixed << std::setprecision(6) << physical_activity << "," // physical_activity
        << static_cast<int>(income_category) << "," << step_name << "," << std::fixed
        << std::setprecision(6) << value_assigned << "," << expected_value << "," << linear_result
        << "," << residual << "," << stddev << "," << lambda << "," << boxcox_result << ","
        << factor_before_clamp << "," << range_lower << "," << range_upper << ","
        << final_clamped_factor << "," << random_residual_before_cholesky << ","
        << residual_after_cholesky;
    return oss.str();
}

std::string StaticLinearModel::get_inspection_csv_header() {
    return "person_id,gender,age,sector,physical_activity,"
           "income_category,step_name,value_assigned,expected_value,linear_result,residual,"
           "stddev,lambda,boxcox_result,factor_before_clamp,range_lower,range_upper,"
           "final_clamped_factor,random_residual_before_cholesky,residual_after_cholesky";
}

// ===== END MAHIMA: Risk Factor Inspection Implementation =====

} // namespace hgps
