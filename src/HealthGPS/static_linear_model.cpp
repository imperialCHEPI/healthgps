#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Input/model_parser.h"
#include "demographic.h"

#include <ranges>
#include <utility>

namespace hgps {

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {
    // NOTE: Demographic variables (region, ethnicity, income, etc.) are already
    // initialized by the DemographicModule in initialise_population

    // Initialise everyone with risk factors.
    for (auto &person : context.population()) {
        initialise_factors(context, person, context.random());
        initialise_physical_activity(context, person, context.random());
    }

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialise everyone with policies and trends.
    for (auto &person : context.population()) {
        initialise_policies(person, context.random(), false);
        initialise_trends(context, person);
    }

    // Adjust such that trended risk factor means match trended expected values.
    adjust_risk_factors(context, names_, ranges_, true);
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {
    // HACK: start intervening two years into the simulation.
    bool intervene = (context.scenario().type() == ScenarioType::intervention &&
                      (context.time_now() - context.start_time()) >= 2);

    // NOTE: Demographic variables are updated by the DemographicModule in update_population
    // Here we only handle risk factor related updates

    // Update risk factors for all people, initializing for newborns.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            // For newborns, initialize demographic variables
            initialise_sector(person, context.random());
            // Demographic variables (region, ethnicity, income) are initialized by the DemographicModule
            initialise_factors(context, person, context.random());
            initialise_physical_activity(context, person, context.random());
        } else {
            // For existing people, only update sector and factors
            update_sector(person, context.random());
            update_factors(context, person, context.random());
        }
    }

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Update policies and trends for all people, initializing for newborns.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            initialise_policies(person, context.random(), intervene);
            initialise_trends(context, person);
        } else {
            update_policies(person, intervene);
            update_trends(context, person);
        }
    }

    // Adjust such that trended risk factor means match trended expected values.
    adjust_risk_factors(context, names_, ranges_, true);

    // Apply policies if intervening.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        apply_policies(person, intervene);
    }
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
// Physical activity depends on age. gender, region, ethncity, income_continuous and random noise
// with std dev Loaded from the static_model.json under the Physical Activity Models section
void StaticLinearModel::initialise_physical_activity([[maybe_unused]] RuntimeContext &context,
                                                     Person &person, Random &random) const {
    // Use the physical activity model we loaded from the JSON
    if (physical_activity_models_.contains("continuous")) {
        const auto &model = physical_activity_models_.at("continuous");

        // Start with the intercept
        double value = model.intercept;

        // Add all coefficient effects
        for (const auto &[factor_name, coefficient] : model.coefficients) {
            // Apply each coefficient to the person's factor value
            value += coefficient * person.get_risk_factor_value(factor_name);
        }
        // Get the standard deviation from the model if available
        double pa_stddev = 0.0;
        if (model.coefficients.count("StandardDeviation") > 0) {
            pa_stddev = model.coefficients.at("StandardDeviation");
        }
        // Add random noise using the standard deviation
        double noise = random.next_normal(0.0, pa_stddev);
        double final_value = value * (1.0 + noise);

        // Set the physical activity value
        person.risk_factors["PhysicalActivity"_id] = final_value;
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
    std::unordered_map<core::Identifier,
                       std::unordered_map<core::Gender, std::unordered_map<core::Region, double>>>
        region_prevalence,
    std::unordered_map<
        core::Identifier,
        std::unordered_map<core::Gender, std::unordered_map<core::Ethnicity, double>>>
        ethnicity_prevalence,
    std::unordered_map<core::Income, LinearModelParams> income_models,
    std::unordered_map<core::Region, LinearModelParams> region_models,
    double physical_activity_stddev,
    const std::unordered_map<core::Identifier, LinearModelParams> &physical_activity_models)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps)},
      expected_trend_boxcox_{std::move(expected_trend_boxcox)}, names_{std::move(names)},
      models_{std::move(models)}, ranges_{std::move(ranges)}, lambda_{std::move(lambda)},
      stddev_{std::move(stddev)}, cholesky_{std::move(cholesky)},
      policy_models_{std::move(policy_models)}, policy_ranges_{std::move(policy_ranges)},
      policy_cholesky_{std::move(policy_cholesky)}, trend_models_{std::move(trend_models)},
      trend_ranges_{std::move(trend_ranges)}, trend_lambda_{std::move(trend_lambda)},
      info_speed_{info_speed}, rural_prevalence_{std::move(rural_prevalence)},
      region_prevalence_{std::move(region_prevalence)},
      ethnicity_prevalence_{std::move(ethnicity_prevalence)},
      income_models_{std::move(income_models)}, region_models_{std::move(region_models)},
      physical_activity_stddev_{physical_activity_stddev},
      physical_activity_models_{physical_activity_models} {

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
        trend_models_, trend_ranges_, trend_lambda_, info_speed_, rural_prevalence_,
        region_prevalence_, ethnicity_prevalence_, income_models_, region_models_,
        physical_activity_stddev_, physical_activity_models_);
}

} // namespace hgps
