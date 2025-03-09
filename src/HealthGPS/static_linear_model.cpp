#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/forward_type.h"
#include "person.h"
#include "runtime_context.h"
#include <random>

#include <ranges>
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
    std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models,
    double physical_activity_stddev, double income_continuous_stddev,
    std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> ethnicity_models)
    : RiskFactorAdjustableModel{std::move(expected), std::move(expected_trend),
                                std::move(trend_steps)},
      expected_trend_boxcox_{std::move(expected_trend_boxcox)}, names_{names}, models_{models},
      ranges_{ranges}, lambda_{lambda}, stddev_{stddev}, cholesky_{cholesky},
      policy_models_{policy_models}, policy_ranges_{policy_ranges},
      policy_cholesky_{policy_cholesky}, trend_models_{std::move(trend_models)},
      trend_ranges_{std::move(trend_ranges)}, trend_lambda_{std::move(trend_lambda)},
      info_speed_{info_speed}, rural_prevalence_{rural_prevalence}, income_models_{income_models},
      region_models_{std::move(region_models)}, physical_activity_stddev_{physical_activity_stddev},
      income_continuous_stddev_{income_continuous_stddev},
      ethnicity_models_{std::move(ethnicity_models)} {
    if (!region_models_) {
        throw core::HgpsException("Region models pointer is null");
    }
    if (!ethnicity_models_) {
        throw core::HgpsException("Ethnicity models pointer is null");
    }
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

// Modified: Mahima 25/02/2025
// Ensuring correct initialization order for population characteristics
void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    // Step 1: Age and gender are already initialized by the population generator (demographic.cpp)
    // also in person.cpp, the person class maintains a deep copy of it

    // Step 2: Initialize fixed characteristics (region and ethnicity)
    for (auto &person : context.population()) {
        // Region depends on the age/gender probabilities
        initialise_region(context, person, context.random());
        // Ethnicity depends on age/gender/region probabilities
        initialise_ethnicity(context, person, context.random());
    }

    // Step 3: Initialize continuous income (depends on age, gender, region, ethnicity)
    for (auto &person : context.population()) {
        initialise_income_continuous(person, context.random());
    }

    // Step 4: Initialize income category based on income_continuous quartiles
    // initialized at the start and then updated every 5 years
    for (auto &person : context.population()) {
        initialise_income_category(person, context.population());
    }

    // Step 5: Initialize physical activity (depends on age, gender, region, ethnicity, income)
    for (auto &person : context.population()) {
        initialise_physical_activity(context, person, context.random());
    }

    // Step 6: Initialize remaining risk factors
    for (auto &person : context.population()) {
        initialise_sector(person, context.random());
        initialise_factors(context, person, context.random());
    }

    // Adjust such that risk factor means match expected values
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialize trends
    for (auto &person : context.population()) {
        initialise_policies(person, context.random(), false);
        initialise_trends(context, person);
    }

    // Adjust such that trended risk factor means match trended expected values
    adjust_risk_factors(context, names_, ranges_, true);
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
            initialise_region(context, person, context.random());
            initialise_ethnicity(context, person, context.random());
            initialise_income_continuous(person, context.random());
            initialise_income_category(person, context.population());
            initialise_factors(context, person, context.random());
        } else {
            update_sector(person, context.random());
            update_region(context, person, context.random());
            update_income_continuous(person, context.random());
            update_income_category(context);
            update_factors(context, person, context.random());
        }
    }

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialise newborns and update others.
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

// Modified: Mahima 25/02/2025
// Physical activity is initialised using the expected value of physical activity based on age,
// gender, region, ethnicity and income
void StaticLinearModel::initialise_physical_activity(RuntimeContext &context, Person &person,
                                                     Random &random) const {
    // Calculate base expected PA value for age and gender
    double expected = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                   std::nullopt, false);

    // Apply modifiers based on region
    const auto &region_params = region_models_->at(person.region);
    double region_effect = region_params.coefficients.at("PhysicalActivity"_id);
    expected *= (1.0 + region_effect);

    // Apply modifiers based on ethnicity
    const auto &ethnicity_params = ethnicity_models_->at(person.ethnicity);
    double ethnicity_effect = ethnicity_params.coefficients.at("PhysicalActivity"_id);
    expected *= (1.0 + ethnicity_effect);

    // Apply modifiers based on continuous income - using all income models
    double income_effect = 0.0;
    for (const auto &[income_level, model] : income_models_) {
        income_effect += model.coefficients.at("PhysicalActivity") * person.income_continuous;
    }
    expected *= (1.0 + income_effect);

    // Add random variation using normal distribution
    double rand = random.next_normal(0.0, physical_activity_stddev_);

    // Set the physical activity value
    person.risk_factors["PhysicalActivity"_id] = expected * (1.0 + rand);
}
// Modified: Mahima 25/02/2025
// Region is initialised using the CDF of the region probabilities along with age/gender strata
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
// This method doesn't use any class members, so it can be static
void StaticLinearModel::initialise_region(RuntimeContext &context, Person &person, Random &random) {
    // Get probabilities for this age/sex stratum
    auto region_probs = context.get_region_probabilities(person.age, person.gender);

    double rand_value = random.next_double();
    double cumulative_prob = 0.0;

    for (const auto &[region, prob] : region_probs) {
        cumulative_prob += prob;
        if (rand_value < cumulative_prob) {
            person.region = region;
            return;
        }
    }

    // If we reach here, no region was assigned - this indicates an error in probability
    // distribution
    throw core::HgpsException("Failed to assign region: cumulative probabilities do not sum to 1.0 "
                              "or are incorrectly distributed");
}
// NOTE: Might need to change how region is being updated later
void StaticLinearModel::update_region(RuntimeContext &context, Person &person,
                                      Random &random) const {
    if (person.age == 18) {
        initialise_region(context, person, random);
    }
}

// Modified: Mahima 25/02/2025
// Ethnicity is initialised using the CDF of the ethnicity probabilities along with
// age/gender/region strata
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
// This method doesn't use any class members, so it can be static
void StaticLinearModel::initialise_ethnicity(RuntimeContext &context, Person &person,
                                             Random &random) {
    // Get probabilities for this age/sex/region stratum
    auto ethnicity_probs =
        context.get_ethnicity_probabilities(person.age, person.gender, person.region);

    double rand_value = random.next_double();
    double cumulative_prob = 0.0;

    for (const auto &[ethnicity, prob] : ethnicity_probs) {
        cumulative_prob += prob;
        if (rand_value < cumulative_prob) {
            person.ethnicity = ethnicity;
            return;
        }
    }

    // If we reach here, no ethnicity was assigned - this indicates an error in probability
    // distribution
    throw core::HgpsException(
        "Failed to assign ethnicity: cumulative probabilities do not sum to 1.0 "
        "or are incorrectly distributed");
}
// NOTE: Ethnicity has no update ethnicity coz it is fixed after assignment throughout

void StaticLinearModel::initialise_income_continuous(Person &person, Random &random) const {
    // Initialize base income value
    double income_base = 0.0;

    // Add age - apply coefficient to actual age using one model
    income_base += income_models_.begin()->second.coefficients.at("Age") * person.age;

    // Add gender - apply coefficient to binary gender value using one model
    double gender_value = (person.gender == core::Gender::male) ? 1.0 : 0.0;
    income_base += income_models_.begin()->second.coefficients.at("Gender") * gender_value;

    // Add region effect - apply coefficient to region value
    double region_value = person.region_to_value();
    income_base += region_models_->at(person.region).coefficients.at("Income") * region_value;

    // Add ethnicity effect - apply coefficient to ethnicity value
    double ethnicity_value = person.ethnicity_to_value();
    income_base +=
        ethnicity_models_->at(person.ethnicity).coefficients.at("Income") * ethnicity_value;

    // Add random variation with specified standard deviation
    double rand = random.next_normal(0.0, income_continuous_stddev_);

    // Ensure income is positive and apply the random variation
    person.income_continuous = std::max(0.1, income_base * (1.0 + rand));
}

void StaticLinearModel::update_income_continuous(Person &person, Random &random) const {
    if (person.age == 18) {
        initialise_income_continuous(person, random);
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
// This method doesn't use any class members, so it can be static
void StaticLinearModel::initialise_income_category(Person &person, const Population &population) {
    // Same implementation as Kevin Hall Model
    std::vector<double> sorted_incomes;
    sorted_incomes.reserve(population.size());

    // collect all the income_continuous values
    for (const auto &p : population) {
        if (p.income_continuous > 0) {
            sorted_incomes.push_back(p.income_continuous);
        }
    }
    // sort just once (reduces time complexity)
    std::sort(sorted_incomes.begin(), sorted_incomes.end());

    size_t n = sorted_incomes.size();
    double q1_threshold = sorted_incomes[n / 4];
    double q2_threshold = sorted_incomes[n / 2];
    double q3_threshold = sorted_incomes[3 * n / 4];

    if (person.income_continuous <= q1_threshold) {
        person.income_category = core::Income::low;
    } else if (person.income_continuous <= q2_threshold) {
        person.income_category = core::Income::lowermiddle;
    } else if (person.income_continuous <= q3_threshold) {
        person.income_category = core::Income::uppermiddle;
    } else {
        person.income_category = core::Income::high;
    }
}

// done at the start and then every 5 years
void StaticLinearModel::update_income_category(RuntimeContext &context) const {
    static int last_update_year = 0;
    int current_year = context.time_now();

    if (current_year - last_update_year >= 5) {
        for (auto &person : context.population()) {
            if (person.is_active()) {
                initialise_income_category(person, context.population());
            }
        }
        last_update_year = current_year;
    }
}

StaticLinearModelDefinition::StaticLinearModelDefinition(
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    // Using const reference instead of value for better performance
    const std::shared_ptr<std::unordered_map<core::Identifier, double>> &expected_trend_boxcox,
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
    double physical_activity_stddev, double income_continuous_stddev,
    std::unordered_map<core::Ethnicity, LinearModelParams> ethnicity_models)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps)},
      expected_trend_boxcox_{
          std::make_shared<std::unordered_map<core::Identifier, double>>(*expected_trend_boxcox)},
      names_{std::move(names)}, models_{std::move(models)}, ranges_{std::move(ranges)},
      lambda_{std::move(lambda)}, stddev_{std::move(stddev)}, cholesky_{std::move(cholesky)},
      policy_models_{std::move(policy_models)}, policy_ranges_{std::move(policy_ranges)},
      policy_cholesky_{std::move(policy_cholesky)},
      trend_models_{std::make_shared<std::vector<LinearModelParams>>(*trend_models)},
      trend_ranges_{std::make_shared<std::vector<core::DoubleInterval>>(*trend_ranges)},
      trend_lambda_{std::make_shared<std::vector<double>>(*trend_lambda)}, info_speed_{info_speed},
      rural_prevalence_{std::move(rural_prevalence)}, income_models_{std::move(income_models)},
      region_models_{std::make_shared<std::unordered_map<core::Region, LinearModelParams>>(
          std::move(region_models))},
      physical_activity_stddev_{physical_activity_stddev},
      income_continuous_stddev_{income_continuous_stddev},
      ethnicity_models_{std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>(
          std::move(ethnicity_models))} {
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
    if (region_models_->empty()) {
        throw core::HgpsException("Region models mapping is empty");
    }
    for (const auto &name : names_) {
        if (!expected_trend_->contains(name)) {
            throw core::HgpsException("One or more expected trend value is missing");
        }
        if (!trend_steps_->contains(name)) {
            throw core::HgpsException("One or more trend steps value is missing");
        }
        if (!expected_trend_boxcox_->contains(name)) {
            throw core::HgpsException("One or more expected trend BoxCox value is missing");
        }
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    return std::make_unique<StaticLinearModel>(
        std::make_shared<RiskFactorSexAgeTable>(*expected_),
        std::make_shared<std::unordered_map<core::Identifier, double>>(*expected_trend_),
        std::make_shared<std::unordered_map<core::Identifier, int>>(*trend_steps_),
        expected_trend_boxcox_, names_, models_, ranges_, lambda_, stddev_, cholesky_,
        policy_models_, policy_ranges_, policy_cholesky_,
        std::make_shared<std::vector<LinearModelParams>>(*trend_models_),
        std::make_shared<std::vector<core::DoubleInterval>>(*trend_ranges_),
        std::make_shared<std::vector<double>>(*trend_lambda_), info_speed_, rural_prevalence_,
        income_models_,
        std::make_shared<std::unordered_map<core::Region, LinearModelParams>>(*region_models_),
        physical_activity_stddev_, income_continuous_stddev_,
        std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>(
            *ethnicity_models_));
}

} // namespace hgps
