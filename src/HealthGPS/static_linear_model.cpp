#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/forward_type.h"
#include "demographic.h"
#include "person.h"
#include "runtime_context.h"
#include <random>

#include <functional>
#include <iostream>
#include <ranges>
#include <utility>

namespace {
// Common validation function to avoid code duplication
void validate_model_components(
    const std::vector<hgps::core::Identifier> &names,
    const std::vector<hgps::LinearModelParams> &models,
    const std::vector<hgps::core::DoubleInterval> &ranges, const std::vector<double> &lambda,
    const std::vector<double> &stddev, const Eigen::MatrixXd &cholesky,
    const std::vector<hgps::LinearModelParams> &policy_models,
    const std::vector<hgps::core::DoubleInterval> &policy_ranges,
    const Eigen::MatrixXd &policy_cholesky,
    const std::shared_ptr<std::vector<hgps::LinearModelParams>> &trend_models,
    const std::shared_ptr<std::vector<hgps::core::DoubleInterval>> &trend_ranges,
    const std::shared_ptr<std::vector<double>> &trend_lambda,
    const std::unordered_map<hgps::core::Income, hgps::LinearModelParams> &income_models,
    const std::shared_ptr<std::unordered_map<hgps::core::Identifier, double>>
        &expected_trend_boxcox,
    const std::function<bool(const hgps::core::Identifier &)> &expected_trend_checker,
    const std::function<bool(const hgps::core::Identifier &)> &trend_steps_checker) {

    if (names.empty()) {
        throw hgps::core::HgpsException("Risk factor names list is empty");
    }
    if (models.empty()) {
        throw hgps::core::HgpsException("Risk factor model list is empty");
    }
    if (ranges.empty()) {
        throw hgps::core::HgpsException("Risk factor ranges list is empty");
    }
    if (lambda.empty()) {
        throw hgps::core::HgpsException("Risk factor lambda list is empty");
    }
    if (stddev.empty()) {
        throw hgps::core::HgpsException("Risk factor standard deviation list is empty");
    }
    if (!cholesky.allFinite()) {
        throw hgps::core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    if (policy_models.empty()) {
        throw hgps::core::HgpsException("Intervention policy model list is empty");
    }
    if (policy_ranges.empty()) {
        throw hgps::core::HgpsException("Intervention policy ranges list is empty");
    }
    if (!policy_cholesky.allFinite()) {
        throw hgps::core::HgpsException(
            "Intervention policy Cholesky matrix contains non-finite values");
    }
    if (trend_models->empty()) {
        throw hgps::core::HgpsException("Time trend model list is empty");
    }
    if (trend_ranges->empty()) {
        throw hgps::core::HgpsException("Time trend ranges list is empty");
    }
    if (trend_lambda->empty()) {
        throw hgps::core::HgpsException("Time trend lambda list is empty");
    }
    // Rural prevalence can be empty, we'll use defaults if needed
    if (income_models.empty()) {
        throw hgps::core::HgpsException("Income models mapping is empty");
    }
    // Region models can be empty, we'll use defaults if needed

    for (const auto &name : names) {
        // Check expected_trend and trend_steps using the provided checker function
        if (expected_trend_checker(name)) {
            throw hgps::core::HgpsException("One or more expected trend value is missing");
        }
        if (trend_steps_checker(name)) {
            throw hgps::core::HgpsException("One or more trend steps value is missing");
        }
        if (!expected_trend_boxcox->contains(name)) {
            throw hgps::core::HgpsException("One or more expected trend BoxCox value is missing");
        }
    }
}
} // anonymous namespace

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

    // Check for null pointers
    if (!expected_trend_boxcox_) {
        throw core::HgpsException("Expected trend BoxCox pointer is null");
    }

    if (!trend_models_) {
        throw core::HgpsException("Trend models pointer is null");
    }

    if (!trend_ranges_) {
        throw core::HgpsException("Trend ranges pointer is null");
    }

    if (!trend_lambda_) {
        throw core::HgpsException("Trend lambda pointer is null");
    }

    if (!region_models_) {
        throw core::HgpsException("Region models pointer is null");
    }

    if (!ethnicity_models_) {
        throw core::HgpsException("Ethnicity models pointer is null");
    }

    validate();
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

// Modified: Mahima 25/02/2025
// Ensuring correct initialization order for population characteristics
void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {
    context_ = &context; // Store reference to the context
    auto &population = context.population();
    auto &random = context.random();

    // Step 1: Age and gender are already initialized by the population generator (demographic.cpp)
    // also in person.cpp, the person class maintains a deep copy of it

    // Step 2: Initialize fixed characteristics (region and ethnicity)
    // These are now initialized in demographic.cpp

    // Step 3: Initialize continuous income
    // This is now initialized in demographic.cpp

    // Step 4: Initialize income category
    // This is now initialized in demographic.cpp

    // Step 5: Initialize physical activity
    // This is now initialized in demographic.cpp

    // Step 6: Initialize remaining risk factors
    for (auto &person : population) {
        if (person.is_active()) {
            initialise_sector(person, context.random());
            initialise_region(context, person, context.random());
            initialise_ethnicity(context, person, context.random());
            initialise_income_continuous(person, context.random());
            initialise_income_category(person, context.population());
            initialise_physical_activity(context, person, context.random());
            initialise_factors(context, person, context.random());
        }
    }

    // Adjust such that risk factor means match expected values
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialize trends
    for (auto &person : population) {
        if (person.is_active()) {
            initialise_policies(person, random, false);
            initialise_trends(context, person);
        }
    }

    // Adjust such that trended risk factor means match trended expected values
    adjust_risk_factors(context, names_, ranges_, true);
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {

    //std::cout << "DEBUG: [StaticLinearModel] Starting risk factor updates" << std::endl;

    // Store the context reference
    context_ = &context;

    // HACK: start intervening two years into the simulation.
    bool intervene = (context.scenario().type() == ScenarioType::intervention &&
                      (context.time_now() - context.start_time()) >= 2);

    // Add counters to track progress
    int persons_processed = 0;
    int total_persons = context.population().current_active_size();

    // First, ensure all people have valid demographic values from configuration
    ensure_demographic_values(context);

    // Initialise newborns and update others.
        for (auto &person : context.population()) {
                if (!person.is_active()) {
                    continue;
                }

                if (person.age == 0) {
                    // Initialize all demographics for newborns
                    initialise_sector(person, context.random());
                    initialise_region(context, person, context.random());
                    initialise_ethnicity(context, person, context.random());
                    initialise_income_continuous(person, context.random());
                    initialise_income_category(person, context.population());
                    initialise_physical_activity(context, person, context.random());
                    initialise_factors(context, person, context.random());
                } else {
                    // Update demographics for existing individuals
                    update_sector(person, context.random());
                    update_region(context, person, context.random());
                    update_income_continuous(person, context.random());
                    update_income_category(context);
                    update_factors(context, person, context.random());
                } 
        }

        //std::cout << "DEBUG: [StaticLinearModel] Beginning adjust_risk_factors" << std::endl;

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

    void StaticLinearModel::initialise_factors(RuntimeContext & context, Person & person,
                                               Random & random) const {

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

    void StaticLinearModel::update_factors(RuntimeContext & context, Person & person,
                                           Random & random) const {

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

    void StaticLinearModel::initialise_trends(RuntimeContext & context, Person & person) const {

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

    void StaticLinearModel::update_trends(RuntimeContext & context, Person & person) const {

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

    void StaticLinearModel::initialise_policies(Person & person, Random & random, bool intervene)
        const {
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

    void StaticLinearModel::update_policies(Person & person, bool intervene) const {

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

    void StaticLinearModel::apply_policies(Person & person, bool intervene) const {

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

    std::vector<double> StaticLinearModel::compute_linear_models(
        Person & person, const std::vector<LinearModelParams> &models) const {
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

    std::vector<double> StaticLinearModel::compute_residuals(
        Random & random, const Eigen::MatrixXd &cholesky) const {
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
    try {
        // Get rural prevalence for age group and sex.
        double prevalence = 0.5; // Default prevalence value of 50%

        if (person.age < 18) {
            // Check if Under18 exists in the map
            if (rural_prevalence_.count("Under18"_id) > 0) {
                // Check if gender exists in the inner map
                if (rural_prevalence_.at("Under18"_id).count(person.gender) > 0) {
                    prevalence = rural_prevalence_.at("Under18"_id).at(person.gender);
                }
            }
        } else {
            // Check if Over18 exists in the map
            if (rural_prevalence_.count("Over18"_id) > 0) {
                // Check if gender exists in the inner map
                if (rural_prevalence_.at("Over18"_id).count(person.gender) > 0) {
                    prevalence = rural_prevalence_.at("Over18"_id).at(person.gender);
                }
            }
        }

        // Sample the person's sector.
        double rand = random.next_double();
        auto sector = rand < prevalence ? core::Sector::rural : core::Sector::urban;
        person.sector = sector;
    } catch (const std::exception &) {
        person.sector = core::Sector::urban;
    }
}

void StaticLinearModel::update_sector(Person &person, Random &random) const {
    try {
        // Only update rural sector 18 year olds.
        if ((person.age != 18) || (person.sector != core::Sector::rural)) {
            return;
        }

        // Get rural prevalence for age group and sex.
        double prevalence_under18 = 0.5; // Default value
        double prevalence_over18 = 0.5;  // Default value

        // Check if the Under18 age group exists in the map
        if (rural_prevalence_.count("Under18"_id) > 0) {
            // Check if the gender exists in the inner map
            if (rural_prevalence_.at("Under18"_id).count(person.gender) > 0) {
                prevalence_under18 = rural_prevalence_.at("Under18"_id).at(person.gender);
            }
        }

        // Check if the Over18 age group exists in the map
        if (rural_prevalence_.count("Over18"_id) > 0) {
            // Check if the gender exists in the inner map
            if (rural_prevalence_.at("Over18"_id).count(person.gender) > 0) {
                prevalence_over18 = rural_prevalence_.at("Over18"_id).at(person.gender);
            }
        }

        // Compute random rural to urban transition.
        double rand = random.next_double();

        // Avoid division by zero or very small numbers
        if (prevalence_under18 < 0.01) {
            prevalence_under18 = 0.01;
        }

        double p_rural_to_urban = 1.0 - prevalence_over18 / prevalence_under18;

        // Ensure probability is valid (between 0 and 1)
        if (p_rural_to_urban < 0.0 || std::isnan(p_rural_to_urban)) {
            p_rural_to_urban = 0.0;
        } else if (p_rural_to_urban > 1.0) {
            p_rural_to_urban = 1.0;
        }

        if (rand < p_rural_to_urban) {
            person.sector = core::Sector::urban;
        }
    } catch (const std::exception &) {
    }
}

void StaticLinearModel::initialise_region(RuntimeContext &context, Person &person, Random &random) {
    // Always use the demographic module for region initialization
    context.demographic_module().initialise_region(context, person, random);
}

void StaticLinearModel::update_region(RuntimeContext &context, Person &person, Random &random) {
    if (person.age == 18) {
        initialise_region(context, person, random);
    }
}

void StaticLinearModel::initialise_ethnicity(RuntimeContext &context, Person &person,
                                             Random &random) {
    // Always use the demographic module for ethnicity initialization
    context.demographic_module().initialise_ethnicity(context, person, random);
}

void StaticLinearModel::initialise_income_continuous(Person &person, Random &random) const {
    // Always use the demographic module for income_continuous initialization
    context_->demographic_module().initialise_income_continuous(person, random);
}

void StaticLinearModel::update_income_continuous(Person &person, Random &random) const {
    // Removing age check to ensure income is updated for all individuals regardless of age
    // This treats income as household income rather than individual income

    // Call initialization to update the income
    initialise_income_continuous(person, random);
}

void StaticLinearModel::initialise_income_category(Person &person, const Population &population) {
    // Get thresholds from population
    auto [q1_threshold, q2_threshold, q3_threshold] =
        context_->demographic_module().calculate_income_thresholds(population);

    // Forward to demographic module
    context_->demographic_module().initialise_income_category(person, q1_threshold, q2_threshold,
                                                              q3_threshold);
}

// done at the start and then every 5 years
void StaticLinearModel::update_income_category(RuntimeContext &context) {
    static int last_update_year = 0;
    int current_year = context.time_now();

    if (current_year - last_update_year >= 5) {
        // Calculate income thresholds once for the entire population
        auto [q1_threshold, q2_threshold, q3_threshold] =
            context.demographic_module().calculate_income_thresholds(context.population());

        // Validate thresholds are properly ordered
        if (q1_threshold > q2_threshold || q2_threshold > q3_threshold) {
            std::cerr << "ERROR: Income thresholds are incorrectly ordered in "
                         "StaticLinearModel::update_income_category"
                      << std::endl;
            // Fix by using percentages of range
            double min_income = 23.0;
            double max_income = 2375.0;
            double range = max_income - min_income;
            q1_threshold = min_income + range * 0.25;
            q2_threshold = min_income + range * 0.5;
            q3_threshold = min_income + range * 0.75;
            std::cerr << "Using fixed thresholds instead: Q1=" << q1_threshold
                      << ", Q2=" << q2_threshold << ", Q3=" << q3_threshold << std::endl;
        }

        // Track inconsistencies during update
        int inconsistency_count = 0;
        int fixed_count = 0;

        for (auto &person : context.population()) {
            if (person.is_active()) {
                // Store original category for comparison
                auto original_category = person.income_category;

                // Get income value and ensure it's within valid range
                double income_value = std::max(23.0, std::min(2375.0, person.income_continuous));

                // Check if income continuous and category are consistent
                bool is_consistent = true;
                if ((person.income_category == core::Income::low && income_value >= q1_threshold) ||
                    (person.income_category == core::Income::lowermiddle &&
                     (income_value < q1_threshold || income_value >= q2_threshold)) ||
                    (person.income_category == core::Income::uppermiddle &&
                     (income_value < q2_threshold || income_value >= q3_threshold)) ||
                    (person.income_category == core::Income::high && income_value < q3_threshold)) {
                    is_consistent = false;
                    inconsistency_count++;

                    // Option: Resynchronize by fixing the category
                    context.demographic_module().initialise_income_category(
                        person, q1_threshold, q2_threshold, q3_threshold);
                    fixed_count++;
                } else {
                    // Already consistent, no action needed
                }

                // Special rule for edge cases - very low income should never be High
                if (income_value <= 30.0 && person.income_category == core::Income::high) {
                    person.income_category = core::Income::low;
                }

                // Special rule for edge cases - very high income should never be Low
                if (income_value >= 2300.0 && person.income_category == core::Income::low) {
                    person.income_category = core::Income::high;
                }
            }
        }

        last_update_year = current_year;
    }
}

void StaticLinearModel::initialise_physical_activity(RuntimeContext &context, Person &person,
                                                     Random &random) const {
    context.demographic_module().initialise_physical_activity(context, person, random);
}

void StaticLinearModel::validate() const {
    // Define checkers that always return false since we can't access these in this class
    std::function<bool(const core::Identifier &)> expected_trend_checker =
        [](const core::Identifier &) { return false; };
    std::function<bool(const core::Identifier &)> trend_steps_checker =
        [](const core::Identifier &) { return false; };

    validate_model_components(names_, models_, ranges_, lambda_, stddev_, cholesky_, policy_models_,
                              policy_ranges_, policy_cholesky_, trend_models_, trend_ranges_,
                              trend_lambda_, income_models_, expected_trend_boxcox_,
                              expected_trend_checker, trend_steps_checker);
}

StaticLinearModelDefinition::StaticLinearModelDefinition(
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    // Using const reference instead of value for better performance
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
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

    // Validate in constructor using the validate method to avoid duplication
    validate();
}

void StaticLinearModelDefinition::validate() const {
    // Define checkers that check expected_trend_ and trend_steps_
    std::function<bool(const core::Identifier &)> expected_trend_checker =
        [this](const core::Identifier &name) { return !expected_trend_->contains(name); };
    std::function<bool(const core::Identifier &)> trend_steps_checker =
        [this](const core::Identifier &name) { return !trend_steps_->contains(name); };

    validate_model_components(names_, models_, ranges_, lambda_, stddev_, cholesky_, policy_models_,
                              policy_ranges_, policy_cholesky_, trend_models_, trend_ranges_,
                              trend_lambda_, income_models_, expected_trend_boxcox_,
                              expected_trend_checker, trend_steps_checker);
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

void StaticLinearModel::ensure_demographic_values(RuntimeContext &context) {
    //std::cout << "DEBUG: [StaticLinearModel] Checking if people have valid demographic values" << std::endl;

    // Track counts of missing values that we'll fix
    int ethnicity_fixed = 0;
    int region_fixed = 0;
    int income_fixed = 0;
    int physical_activity_fixed = 0;

    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        // Fix unknown ethnicity using the proper demographic module initialization
        if (person.ethnicity == core::Ethnicity::unknown) {
            try {
                context.demographic_module().initialise_ethnicity(context, person,
                                                                  context.random());
                ethnicity_fixed++;
            } catch (const std::exception &e) {
                std::cerr << "ERROR: Failed to initialize ethnicity: " << e.what() << std::endl;
            }
        }

        // Fix unknown region - we can set this to England as specified
        if (person.region == core::Region::unknown) {
            person.region = core::Region::England; // Set default
            region_fixed++;
        }

        // Fix missing income_continuous using the proper demographic module initialization
        if (!person.risk_factors.contains("income_continuous"_id)) {
            try {
                context.demographic_module().initialise_income_continuous(person, context.random());
                income_fixed++;

                // After initializing income_continuous, update the category
                auto [q1_threshold, q2_threshold, q3_threshold] =
                    context.demographic_module().calculate_income_thresholds(context.population());
                context.demographic_module().initialise_income_category(person, q1_threshold,
                                                                        q2_threshold, q3_threshold);
            } catch (const std::exception &e) {
                std::cerr << "ERROR: Failed to initialize income: " << e.what() << std::endl;
            }
        }

        // Fix missing physical activity using the proper demographic module initialization
        if (!person.risk_factors.contains("PhysicalActivity"_id)) {
            try {
                context.demographic_module().initialise_physical_activity(context, person,
                                                                          context.random());
                physical_activity_fixed++;
            } catch (const std::exception &e) {
                std::cerr << "ERROR: Failed to initialize physical activity: " << e.what()
                          << std::endl;
            }
        }
    }
}

} // namespace hgps
