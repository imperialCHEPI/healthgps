#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "population.h"
#include "risk_factor_adjustable_model.h"
#include "runtime_context.h"

#include <algorithm>
#include <cmath>
#include <iostream> // Added for print statements
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
    double physical_activity_stddev, TrendType trend_type,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox,
    std::shared_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps,
    std::shared_ptr<std::vector<LinearModelParams>> income_trend_models,
    std::shared_ptr<std::vector<core::DoubleInterval>> income_trend_ranges,
    std::shared_ptr<std::vector<double>> income_trend_lambda,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors,
    bool is_continuous_income_model, const LinearModelParams &continuous_income_model,
    const std::string &income_categories,
    const std::unordered_map<core::Identifier, PhysicalActivityModel> &physical_activity_models,
    bool has_active_policies)
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    : RiskFactorAdjustableModel{std::move(expected),       expected_trend, trend_steps, trend_type,
                                expected_income_trend,       // Pass by value, not moved
                                income_trend_decay_factors}, // Pass by value, not moved
      // Continuous income model support (FINCH approach) - must come first
      is_continuous_income_model_{is_continuous_income_model},
      continuous_income_model_{continuous_income_model}, income_categories_{income_categories},
      // Regular trend member variables - these are shared_ptr, so we can move them
      expected_trend_{std::move(expected_trend)},
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
      physical_activity_stddev_{physical_activity_stddev},
      physical_activity_models_{physical_activity_models},
      has_physical_activity_models_{!physical_activity_models.empty()},
      has_active_policies_{has_active_policies} {

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
        std::cout << "\nDEBUG: Validating UPF trend parameters...";
        if (trend_models_->empty()) {
            throw core::HgpsException("Time trend model list is empty");
        }
        if (trend_ranges_->empty()) {
            throw core::HgpsException("Time trend ranges list is empty");
        }
        if (trend_lambda_->empty()) {
            throw core::HgpsException("Time trend lambda list is empty");
        }
    } else {
        std::cout << "\nDEBUG: Skipping UPF trend validation (trend_type_ != Trend)";
    }

    // Validate income trend parameters if income trend is enabled
    if (trend_type_ == TrendType::IncomeTrend) {
        std::cout << "\nDEBUG: Validating income trend parameters...";
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
    } else {
        std::cout << "\nDEBUG: Skipping income trend validation (trend_type_ != IncomeTrend)";
    }

    // Validate income trend data consistency
    if (income_trend_models_ && income_trend_models_->empty()) {
        throw core::HgpsException("Income trend model list is empty");
    }
    if (income_trend_ranges_ && income_trend_ranges_->empty()) {
        throw core::HgpsException("Income trend ranges list is empty");
    }
    if (income_trend_lambda_ && income_trend_lambda_->empty()) {
        throw core::HgpsException("Income trend lambda list is empty");
    }

    // Validate that all risk factors have income trend parameters
    // Only validate income trend parameters if income trend is enabled
    if (trend_type_ == TrendType::IncomeTrend && expected_income_trend_) {
        for (const auto &name : names_) {
            std::cout << "\nDEBUG: Checking risk factor: " << name.to_string();
            if (!expected_income_trend_->contains(name)) {
                throw core::HgpsException(
                    "One or more expected income trend value is missing for risk factor: " +
                    name.to_string());
            }
            if (!expected_income_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected income trend BoxCox value is "
                                          "missing for risk factor: " +
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
        std::cout << "\nDEBUG: All risk factor income trend parameters validated successfully";
    } else {
        std::cout << "\nDEBUG: Skipping risk factor income trend parameter validation (trend_type_ "
                     "!= IncomeTrend or expected_income_trend_ is null)";
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
    std::cout << "\nDEBUG: StaticLinearModel constructor completed successfully";
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

bool StaticLinearModel::is_continuous_income_model() const noexcept {
    return is_continuous_income_model_;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {
    std::cout << "\n=== STATIC LINEAR MODEL: GENERATING RISK FACTORS ===";

    // MAHIMA: Initialise everyone. Order is important here.
    // STEP 1: Age and gender initialized in demographic.cpp
    // STEP 2: Region and ethnicity initialized in demographic.cpp (if available)
    // STEP 3: Sector initialized in static_linear_model.cpp (if available)
    // STEP 4: Income initialized in static_linear_model.cpp (batch processing for continuous
    // income) STEP 5: Risk factors and physical activity initialized in static_linear_model.cpp
    // (below) STEP 6: Risk factors adjusted to expected means in static_linear_model.cpp (below)
    // STEP 7: Policies in static_linear_model.cpp (below)
    // STEP 8: Trends in static_linear_model.cpp (below) depends on whether trends are enabled
    // STEP 9: Risk factors adjusted to trended expected means in static_linear_model.cpp
    // (below)

    // STEP 3: Initialize sector for all people (if sector info is available)
    std::cout << "\nInitializing sector for all people...";
    for (auto &person : context.population()) {
        initialise_sector(person, context.random());
    }

    // STEP 4: Initialize income (batch processing for continuous income model)
    if (is_continuous_income_model_) {
        std::cout << "\nInitializing continuous income for all people...";
        // Phase 1: Calculate continuous income for all people
        for (auto &person : context.population()) {
            double continuous_income = calculate_continuous_income(person, context.random());
            person.risk_factors["income_continuous"_id] = continuous_income;
            person.income_continuous = continuous_income;
        }
        // Phase 2: Calculate quartiles once
        std::vector<double> quartile_thresholds = calculate_income_quartiles(context.population());
        // Phase 3: Assign categories using pre-calculated quartiles
        for (auto &person : context.population()) {
            double continuous_income = person.risk_factors.at("income_continuous"_id);
            person.income = convert_income_to_category(continuous_income, quartile_thresholds);
        }
        std::cout << "\nSuccessfully initialized continuous income for all people";
    } else {
        std::cout << "\nInitializing categorical income for all people...";
        for (auto &person : context.population()) {
            initialise_categorical_income(person, context.random());
        }
        std::cout << "\nSuccessfully initialized categorical income for all people";
    }

    // STEP 5: Initialize risk factors and physical activity
    std::cout << "\nInitializing risk factors and physical activity...";
    for (auto &person : context.population()) {
        initialise_factors(context, person, context.random());
        initialise_physical_activity(context, person, context.random());
    }
    std::cout << "\nInitialization order completed: Age -> Gender -> Region -> Ethnicity -> Sector "
                 "-> Income -> Risk Factors -> Physical Activity";

    // Verify that all attributes are properly initialized
    std::cout << "\nVerifying person attributes...";
    int verified_count = 0;
    for (const auto &person : context.population()) {
        if (verified_count < 3) { // Check first 3 people
            std::cout << "\nPerson " << verified_count + 1 << ": Age=" << person.age
                      << ", Gender=" << (person.gender == core::Gender::male ? "male" : "female")
                      << ", Region=" << person.region << ", Ethnicity=" << person.ethnicity
                      << ", Sector=" << (person.sector == core::Sector::urban ? "urban" : "rural")
                      << ", Income=" << static_cast<int>(person.income)
                      << ", RiskFactors=" << person.risk_factors.size()
                      << ", PhysicalActivity=" << person.physical_activity;
        }
        verified_count++;
    }

    // Adjust such that risk factor means match expected values.
    std::cout << "\nStarting risk factor adjustment...";
    adjust_risk_factors(context, names_, ranges_, false);
    std::cout << "\nRisk factor adjustment completed";

    // Initialise everyone with appropriate trend type.
    std::cout << "\nStarting trend initialization...";
    for (auto &person : context.population()) {
        if (has_active_policies_) {
            initialise_policies(person, context.random(), false);
        }

        // Apply trend based on trend_type
        switch (trend_type_) {
        case TrendType::Null:
            // No trends applied
            break;
        case TrendType::Trend:
            initialise_UPF_trends(context, person);
            break;
        case TrendType::IncomeTrend:
            initialise_income_trends(context, person);
            break;
        }
    }
    std::cout << "\nTrend initialization completed";

    // Adjust such that trended risk factor means match trended expected values.
    // Only apply trend adjustment if we have a trend type
    if (trend_type_ != TrendType::Null) {
        std::cout << "\nStarting trended risk factor adjustment...";
        adjust_risk_factors(context, names_, ranges_, true);
        std::cout << "\nTrended risk factor adjustment completed";
    } else {
        std::cout << "\nSkipping trended adjustment (trend_type_ is Null)";
    }

    std::cout << "\n=== STATIC LINEAR MODEL: RISK FACTOR GENERATION COMPLETED ===";
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
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
            initialise_income(context, person, context.random());
            initialise_factors(context, person, context.random());
            initialise_physical_activity(context, person, context.random());
        } else {
            update_sector(person, context.random());
            update_income(context, person, context.random());
            update_factors(context, person, context.random());
        }
    }

    // Adjust such that risk factor means match expected values(factor mean).
    adjust_risk_factors(context, names_, ranges_, false);

    // Initialise newborns and update others with appropriate trend type.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            if (has_active_policies_) {
                initialise_policies(person, context.random(), intervene);
            }

            // Apply trend based on trend_type
            switch (trend_type_) {
            case TrendType::Null:
                // No trends applied
                break;
            case TrendType::Trend:
                initialise_UPF_trends(context, person);
                break;
            case TrendType::IncomeTrend:
                initialise_income_trends(context, person);
                break;
            }
        } else {
            if (has_active_policies_) {
                update_policies(person, intervene);
            }

            // Apply trend based on trend_type
            switch (trend_type_) {
            case TrendType::Null:
                // No trends applied
                break;
            case TrendType::Trend:
                update_UPF_trends(context, person);
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
    if (has_active_policies_) {
        for (auto &person : context.population()) {
            if (!person.is_active()) {
                continue;
            }

            apply_policies(person, intervene);
        }
    }
}

double StaticLinearModel::inverse_box_cox(double factor, double lambda) {
    double base = (lambda * factor) + 1.0;
    double result = pow(base, 1.0 / lambda);
    return result;
}

void StaticLinearModel::initialise_factors(RuntimeContext &context, Person &person,
                                           Random &random) const {
    static int factors_count = 0;
    static bool first_call = true;
    factors_count++;

    if (first_call) {
        std::cout << "\nStarting risk factors initialization...";
        first_call = false;
    }
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

    if (factors_count % 5000 == 0) {
        std::cout << "\nSuccessfully initialized risk factors for " << factors_count << " people";
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
void StaticLinearModel::initialise_UPF_trends(RuntimeContext &context, Person &person) const {

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
    update_UPF_trends(context, person);
}

// This function is for updating UPF Trends
void StaticLinearModel::update_UPF_trends(RuntimeContext &context, Person &person) const {

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
    // Mahima's enhancement: Skip ALL policy initialization if policies are zero
    if (!has_active_policies_) {
        return; // Skip Cholesky decomposition, residual storage, everything!
    }

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
    // Mahima's enhancement: Skip policy computation if all policies are zero
    if (!has_active_policies_) {
        return;
    }

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
    // Mahima's enhancement: Skip policy application if all policies are zero
    if (!has_active_policies_) {
        return;
    }

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
            try {
                double value = person.get_risk_factor_value(coefficient_name);
                factor += coefficient_value * value;
            } catch (const std::exception &e) {
                std::cout << "\nERROR: Coefficient '" << coefficient_name.to_string()
                          << "' not found: " << e.what();
                std::cout << "\nPerson attributes: Age=" << person.age << ", Gender="
                          << (person.gender == core::Gender::male ? "male" : "female")
                          << ", Region=" << person.region << ", Ethnicity=" << person.ethnicity
                          << ", Income=" << static_cast<int>(person.income);
                throw;
            }
        }
        for (const auto &[coefficient_name, coefficient_value] : model.log_coefficients) {
            try {
                double value = person.get_risk_factor_value(coefficient_name);
                factor += coefficient_value * log(value);
            } catch (const std::exception &e) {
                std::cout << "\nERROR: Log coefficient '" << coefficient_name.to_string()
                          << "' not found: " << e.what();
                throw;
            }
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
    // If no rural prevalence data is available, skip sector assignment
    if (rural_prevalence_.empty()) {
        return;
    }

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
    // If no rural prevalence data is available, skip sector update
    if (rural_prevalence_.empty()) {
        return;
    }

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

void StaticLinearModel::initialise_income(RuntimeContext &context, Person &person, Random &random) {
    if (is_continuous_income_model_) {
        // FINCH approach: Calculate continuous income for a single person (e.g., newborns)
        double continuous_income = calculate_continuous_income(person, context.random());
        person.risk_factors["income_continuous"_id] = continuous_income;
        person.income_continuous = continuous_income;
        // Recalculate quartiles for updated population and assign category
        std::vector<double> quartile_thresholds = calculate_income_quartiles(context.population());
        person.income = convert_income_to_category(continuous_income, quartile_thresholds);
    } else {
        // India approach: Use direct categorical assignment
        initialise_categorical_income(person, random);
    }
}

void StaticLinearModel::update_income(RuntimeContext &context, Person &person, Random &random) {
    // Only update 18 year olds
    if (person.age == 18) {
        initialise_income(context, person, random);
    }
}

// MAHIMA: This is the India approach for categorical income assignment
// In the static_model.json, we have a set of IncomeModels as categories (e.g., low, medium,
// high)
void StaticLinearModel::initialise_categorical_income(Person &person, Random &random) {
    // India approach: Direct categorical assignment using logits and softmax
    // Compute logits for each income category
    auto logits = std::unordered_map<core::Income, double>{};
    for (const auto &[income, params] : income_models_) {
        logits[income] = params.intercept;
        for (const auto &[factor, coefficient] : params.coefficients) {
            logits.at(income) += coefficient * person.get_risk_factor_value(factor);
        }
    }

    // Compute softmax probabilities for each income category
    auto e_logits = std::unordered_map<core::Income, double>{};
    double e_logits_sum = 0.0;
    for (const auto &[income, logit] : logits) {
        e_logits[income] = exp(logit);
        e_logits_sum += e_logits.at(income);
    }

    // Compute income category probabilities
    auto probabilities = std::unordered_map<core::Income, double>{};
    for (const auto &[income, e_logit] : e_logits) {
        probabilities[income] = e_logit / e_logits_sum;
    }

    // Assign income category using CDF
    double rand = random.next_double();
    double cumulative_prob = 0.0;
    for (const auto &[income, probability] : probabilities) {
        cumulative_prob += probability;
        if (rand < cumulative_prob) {
            person.income = income;
            return;
        }
    }

    throw core::HgpsException("Logic Error: failed to initialise categorical income category");
}

// MAHIMA: This is the FINCH approach for continuous income calculation
// In the static_model.json, we have a IncomeModel with intercept, regression values (age,
// gender, region, ethnicity etc) If is_continuous_income_model_ is true, we use this approach-
// the order for this is- calculate continuous income, calculate quartiles, assign category
void StaticLinearModel::initialise_continuous_income(RuntimeContext &context, Person &person,
                                                     Random &random) {
    // FINCH approach: Calculate continuous income, then convert to category
    // Step 1: Calculate continuous income
    double continuous_income = calculate_continuous_income(person, random);

    // Store continuous income in risk factors for future use
    person.risk_factors["income_continuous"_id] = continuous_income;

    // Step 2: Convert to income category based on population quartiles
    person.income =
        convert_income_continuous_to_category(continuous_income, context.population(), random);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
double StaticLinearModel::calculate_continuous_income(Person &person, Random &random) {
    // Calculate continuous income using the continuous income model
    double income = continuous_income_model_.intercept;

    // Add coefficient contributions for each person attribute
    for (const auto &[factor, coefficient] : continuous_income_model_.coefficients) {
        std::string factor_name = factor.to_string();

        // Skip special coefficients that are not part of the regression
        if (factor_name == "IncomeContinuousStdDev" || factor_name == "stddev" ||
            factor_name == "min" || factor_name == "max") {
            continue;
        }

        // Dynamic coefficient matching based on factor name patterns
        double factor_value = 0.0;

        // Age effects - handle age, age2, age3, etc. dynamically
        if (factor_name.starts_with("age")) {
            if (factor_name == "age") {
                factor_value = static_cast<double>(person.age);
            } else if (factor_name == "age2") {
                factor_value = person.age * person.age;
            } else if (factor_name == "age3") {
                factor_value = person.age * person.age * person.age;
            } else {
                // Handle age4, age5, etc. dynamically
                int power = 1;
                if (factor_name.length() > 3) {
                    try {
                        power = std::stoi(factor_name.substr(3));
                    } catch (...) {
                        std::cout << "Warning: Could not parse age power from factor name: "
                                  << factor_name << '\n';
                        continue;
                    }
                }
                factor_value = std::pow(person.age, power);
            }
        }
        // Gender effects - handle gender, gender2, etc. dynamically
        else if (factor_name.starts_with("gender")) {
            if (factor_name == "gender") {
                factor_value = person.gender_to_value();
            } else if (factor_name == "gender2") {
                factor_value = person.gender == core::Gender::male ? 1.0 : 0.0;
            } else {
                // Handle gender3, gender4, etc. dynamically
                int power = 1;
                if (factor_name.length() > 6) {
                    try {
                        power = std::stoi(factor_name.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse gender power from factor name: "
                                  << factor_name << '\n';
                        continue;
                    }
                }
                double base_value = person.gender == core::Gender::male ? 1.0 : 0.0;
                factor_value = std::pow(base_value, power);
            }
        }
        // Region effects - handle region2, region3, region56, etc. dynamically
        else if (factor_name.starts_with("region")) {
            if (factor_name == "region") {
                // If person.region is a number, use it directly
                try {
                    factor_value = std::stod(person.region);
                } catch (...) {
                    // If not a number, check if it matches the factor name
                    factor_value =
                        (person.region == factor_name)
                            ? 1.0
                            : 0.0; // this shows if the factor value is 1 (available )then use
                                   // else if it is 0 (not available) then move on.
                }
            } else {
                // Handle region2, region3, region56, etc. dynamically
                // std::string region_number = factor_name.substr(6); // Remove "region" prefix
                factor_value = (person.region == factor_name) ? 1.0 : 0.0;
            }
        }
        // Ethnicity effects - handle ethnicity2, ethnicity3, ethnicity90, etc. dynamically
        else if (factor_name.starts_with("ethnicity")) {
            if (factor_name == "ethnicity") {
                // If person.ethnicity is a number, use it directly
                try {
                    factor_value = std::stod(person.ethnicity);
                } catch (...) {
                    // If not a number, check if it matches the factor name
                    factor_value = (person.ethnicity == factor_name) ? 1.0 : 0.0;
                }
            } else {
                // Handle ethnicity2, ethnicity3, ethnicity90, etc. dynamically
                // std::string ethnicity_number = factor_name.substr(9); // Remove "ethnicity"
                // prefix
                factor_value = (person.ethnicity == factor_name) ? 1.0 : 0.0;
            }
        }
        // Sector effects - handle sector, sector2, etc. dynamically
        else if (factor_name.starts_with("sector")) {
            if (factor_name == "sector") {
                factor_value = person.sector_to_value();
            } else {
                // Handle sector2, sector3, etc. dynamically
                int power = 1;
                if (factor_name.length() > 6) {
                    try {
                        power = std::stoi(factor_name.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse sector power from factor name: "
                                  << factor_name << '\n';
                        continue;
                    }
                }
                double base_value = person.sector_to_value();
                factor_value = std::pow(base_value, power);
            }
        }
        // Income effects - handle income, income2, etc. dynamically
        else if (factor_name.starts_with("income")) {
            if (factor_name == "income") {
                factor_value = static_cast<double>(person.income);
            } else if (factor_name == "income_continuous") {
                factor_value = person.income_continuous;
            } else {
                // Handle income2, income3, etc. dynamically
                int power = 1;
                if (factor_name.length() > 6) {
                    try {
                        power = std::stoi(factor_name.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse income power from factor name: "
                                  << factor_name << '\n';
                        continue;
                    }
                }
                auto base_value = static_cast<double>(person.income);
                factor_value = std::pow(base_value, power);
            }
        }
        // Region value effects - handle any region values dynamically
        else if (factor_name == person.region || factor_name == person.ethnicity) {
            // Check if person's region or ethnicity matches this factor name exactly
            factor_value = 1.0;
        }
        // Risk factor effects - try to get from risk factors
        else {
            try {
                factor_value = person.get_risk_factor_value(factor);
            } catch (...) {
                // Factor not found, skip it
                std::cout << "Warning: Factor " << factor_name
                          << " not found for continuous income calculation, skipping.\n";
                continue;
            }
        }

        // Add the coefficient contribution
        income += coefficient * factor_value;
    }

    // Add random noise based on standard deviation
    double stddev = 0.0;
    auto stddev_it = continuous_income_model_.coefficients.find("IncomeContinuousStdDev"_id);
    if (stddev_it != continuous_income_model_.coefficients.end()) {
        stddev = stddev_it->second;
    }

    if (stddev > 0.0) {
        double noise = random.next_normal(0.0, stddev);
        // Store the noise as a residual for potential future updates
        if (person.risk_factors.contains("income_continuous_residual"_id)) {
            person.risk_factors.at("income_continuous_residual"_id) = noise;
        } else {
            person.risk_factors.emplace("income_continuous_residual"_id, noise);
        }
        income += noise;
    }

    // Apply min/max bounds if they exist
    auto min_it = continuous_income_model_.coefficients.find("min"_id);
    auto max_it = continuous_income_model_.coefficients.find("max"_id);

    if (min_it != continuous_income_model_.coefficients.end()) {
        income = std::max(income, min_it->second);
    }
    if (max_it != continuous_income_model_.coefficients.end()) {
        income = std::min(income, max_it->second);
    }

    return income;
}

std::vector<double> StaticLinearModel::calculate_income_quartiles(const Population &population) {
    // Collect all valid continuous income values from the population
    std::vector<double> sorted_incomes;
    sorted_incomes.reserve(population.size());

    for (const auto &person : population) {
        if (person.is_active()) {
            auto it = person.risk_factors.find("income_continuous"_id);
            if (it != person.risk_factors.end()) {
                sorted_incomes.push_back(it->second);
            }
        }
    }

    if (sorted_incomes.empty()) {
        throw core::HgpsException(
            "No continuous income values found in population for quartile calculation");
    }

    // Sort to find quartile thresholds
    std::ranges::sort(sorted_incomes);

    size_t n = sorted_incomes.size();
    std::vector<double> quartile_thresholds(3);

    // Calculate exact quartile boundaries for 25%, 50%, 75%
    size_t q1_index = n / 4;
    size_t q2_index = n / 2;
    size_t q3_index = (3 * n) / 4;

    // Ensure proper boundaries and avoid out-of-bounds access
    if (q1_index > 0 && q1_index < n) {
        quartile_thresholds[0] = sorted_incomes[q1_index - 1]; // 25th percentile
    } else {
        quartile_thresholds[0] = sorted_incomes.front();
    }

    if (q2_index > 0 && q2_index < n) {
        quartile_thresholds[1] = sorted_incomes[q2_index - 1]; // 50th percentile
    } else {
        quartile_thresholds[1] = sorted_incomes[n / 2];
    }

    if (q3_index > 0 && q3_index < n) {
        quartile_thresholds[2] = sorted_incomes[q3_index - 1]; // 75th percentile
    } else {
        quartile_thresholds[2] = sorted_incomes.back();
    }

    return quartile_thresholds;
}

core::Income StaticLinearModel::convert_income_continuous_to_category(double continuous_income,
                                                                      const Population &population,
                                                                      Random & /*random*/) const {
    // Calculate quartiles from the current population
    std::vector<double> quartile_thresholds = calculate_income_quartiles(population);

    if (income_categories_ == "4") {
        // 4-category system: low, lowermiddle, uppermiddle, high
        if (continuous_income <= quartile_thresholds[0]) {
            return core::Income::low;
        }
        if (continuous_income <= quartile_thresholds[1]) {
            return core::Income::lowermiddle;
        }
        if (continuous_income <= quartile_thresholds[2]) {
            return core::Income::uppermiddle;
        }
        return core::Income::high;
    } else {
        // 3-category system: low, middle, high
        // For 3 categories, we'll use the 33rd and 67th percentiles
        if (continuous_income <= quartile_thresholds[0]) {
            return core::Income::low;
        }
        if (continuous_income <= quartile_thresholds[2]) {
            return core::Income::middle;
        }
        return core::Income::high;
    }
}

// Optimized version: uses pre-calculated quartiles (no population scan)
core::Income StaticLinearModel::convert_income_to_category(
    double continuous_income, const std::vector<double> &quartile_thresholds) const {
    if (income_categories_ == "4") {
        // 4-category system: low, lowermiddle, uppermiddle, high
        if (continuous_income <= quartile_thresholds[0]) {
            return core::Income::low;
        }
        if (continuous_income <= quartile_thresholds[1]) {
            return core::Income::lowermiddle;
        }
        if (continuous_income <= quartile_thresholds[2]) {
            return core::Income::uppermiddle;
        }
        return core::Income::high;
    } else {
        // 3-category system: low, middle, high
        if (continuous_income <= quartile_thresholds[0]) {
            return core::Income::low;
        }
        if (continuous_income <= quartile_thresholds[2]) {
            return core::Income::middle;
        }
        return core::Income::high;
    }
}

void StaticLinearModel::initialise_physical_activity(RuntimeContext &context, Person &person,
                                                     Random &random) const {
    static int physical_activity_count = 0;
    static bool first_call = true;
    physical_activity_count++;

    if (first_call) {
        std::cout << "\nStarting physical activity initialization...";
        first_call = false;
    }

    // Check if we have physical activity models
    if (has_physical_activity_models_) {
        // Check which type of model we have
        auto simple_model = physical_activity_models_.find(core::Identifier("simple"));
        auto continuous_model = physical_activity_models_.find(core::Identifier("continuous"));

        if (simple_model != physical_activity_models_.end()) {
            // India approach: Simple model with standard deviation
            initialise_simple_physical_activity(context, person, random, simple_model->second);
        } else if (continuous_model != physical_activity_models_.end()) {
            // FINCH approach: Continuous model with CSV file loading
            initialise_continuous_physical_activity(context, person, random,
                                                    continuous_model->second);
        }
    } else {
        // No physical activity models configured - use simple approach with global stddev (India
        // approach)
        PhysicalActivityModel simple_model;
        simple_model.model_type = "simple";
        simple_model.stddev = physical_activity_stddev_;
        initialise_simple_physical_activity(context, person, random, simple_model);
    }

    if (physical_activity_count % 5000 == 0) {
        std::cout << "\nSuccessfully initialized physical activity for " << physical_activity_count
                  << " people";
    }
}

// MAHIMA: Function to initialise continuous physical activity model using regression.
// This is for Finch or any project that has region, ethnicity, income continuous etc in the CSV
// file.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void StaticLinearModel::initialise_continuous_physical_activity(
    [[maybe_unused]] RuntimeContext &context, Person &person, Random &random,
    const PhysicalActivityModel &model) {
    // Start with the intercept
    double value = model.intercept;

    // Process coefficients dynamically from CSV file
    for (const auto &[factor_name, coefficient] : model.coefficients) {
        std::string factor_name_str = factor_name.to_string();

        // Skip special coefficients that are not part of the regression
        if (factor_name_str == "stddev" || factor_name_str == "min" || factor_name_str == "max") {
            continue;
        }

        // Dynamic coefficient matching based on factor name patterns
        double factor_value = 0.0;

        // Age effects - handle age, age2, age3, etc. dynamically
        if (factor_name_str.starts_with("age")) {
            if (factor_name_str == "age") {
                factor_value = static_cast<double>(person.age);
            } else if (factor_name_str == "age2") {
                factor_value = person.age * person.age;
            } else if (factor_name_str == "age3") {
                factor_value = person.age * person.age * person.age;
            } else {
                // Handle age4, age5, etc. dynamically
                int power = 1;
                if (factor_name_str.length() > 3) {
                    try {
                        power = std::stoi(factor_name_str.substr(3));
                    } catch (...) {
                        std::cout << "Warning: Could not parse age power from factor name: "
                                  << factor_name_str << '\n';
                        continue;
                    }
                }
                factor_value = std::pow(person.age, power);
            }
        }
        // Gender effects - handle gender, gender2, etc. dynamically
        else if (factor_name_str.starts_with("gender")) {
            if (factor_name_str == "gender") {
                factor_value = person.gender_to_value();
            } else if (factor_name_str == "gender2") {
                factor_value = person.gender == core::Gender::male ? 1.0 : 0.0;
            } else {
                // Handle gender3, gender4, etc. dynamically
                int power = 1;
                if (factor_name_str.length() > 6) {
                    try {
                        power = std::stoi(factor_name_str.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse gender power from factor name: "
                                  << factor_name_str << '\n';
                        continue;
                    }
                }
                double base_value = person.gender == core::Gender::male ? 1.0 : 0.0;
                factor_value = std::pow(base_value, power);
            }
        }
        // Region effects - handle region2, region3, region56, etc. dynamically
        else if (factor_name_str.starts_with("region")) {
            if (factor_name_str == "region") {
                // If person.region is a number, use it directly
                try {
                    factor_value = std::stod(person.region);
                } catch (...) {
                    // If not a number, check if it matches the factor name
                    factor_value = (person.region == factor_name_str) ? 1.0 : 0.0;
                }
            } else {
                // Handle region2, region3,.... region56, etc. dynamically
                factor_value = (person.region == factor_name_str) ? 1.0 : 0.0;
            }
        }
        // Ethnicity effects - handle ethnicity2, ethnicity3, ethnicity90, etc. dynamically
        else if (factor_name_str.starts_with("ethnicity")) {
            if (factor_name_str == "ethnicity") {
                // If person.ethnicity is a number, use it directly
                try {
                    factor_value = std::stod(person.ethnicity);
                } catch (...) {
                    // If not a number, check if it matches the factor name
                    factor_value = (person.ethnicity == factor_name_str) ? 1.0 : 0.0;
                }
            } else {
                // Handle ethnicity2, ethnicity3, ethnicity90, etc. dynamically
                // std::string ethnicity_number =
                //     factor_name_str.substr(9); // Remove "ethnicity" prefix
                factor_value = (person.ethnicity == factor_name_str) ? 1.0 : 0.0;
            }
        }
        // Sector effects - handle sector, sector2, etc. dynamically
        else if (factor_name_str.starts_with("sector")) {
            if (factor_name_str == "sector") {
                factor_value = person.sector_to_value();
            } else {
                // Handle sector2, sector3, etc. dynamically
                int power = 1;
                if (factor_name_str.length() > 6) {
                    try {
                        power = std::stoi(factor_name_str.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse sector power from factor name: "
                                  << factor_name_str << '\n';
                        continue;
                    }
                }
                double base_value = person.sector_to_value();
                factor_value = std::pow(base_value, power);
            }
        }
        // Income effects - handle income, income2, etc. dynamically
        else if (factor_name_str.starts_with("income")) {
            if (factor_name_str == "income") {
                factor_value = static_cast<double>(person.income);
            } else if (factor_name_str == "income_continuous") {
                factor_value = person.income_continuous;
            } else {
                // Handle income2, income3, etc. dynamically
                int power = 1;
                if (factor_name_str.length() > 6) {
                    try {
                        power = std::stoi(factor_name_str.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse income power from factor name: "
                                  << factor_name_str << '\n';
                        continue;
                    }
                }
                auto base_value = static_cast<double>(person.income);
                factor_value = std::pow(base_value, power);
            }
        }
        // Region value effects - handle any region values dynamically
        else if (factor_name_str == person.region || factor_name_str == person.ethnicity) {
            // Check if person's region or ethnicity matches this factor name exactly
            factor_value = 1.0;
        }
        // Risk factor effects - try to get from risk factors
        else {
            try {
                factor_value = person.get_risk_factor_value(factor_name);
            } catch (...) {
                // Factor not found, skip it
                std::cout << "Warning: Factor " << factor_name_str
                          << " not found for continuous physical activity calculation, skipping.\n";
                continue;
            }
        }

        // Add the coefficient contribution
        value += coefficient * factor_value;
    }

    // Add random noise using the model's standard deviation
    double rand_noise = random.next_normal(0.0, model.stddev);
    double final_value = value + rand_noise;

    // Apply min/max constraints
    final_value = std::max(model.min_value, std::min(final_value, model.max_value));

    // Set the physical activity value (store in both member variable and risk_factors for compatibility)
    person.physical_activity = final_value;
    person.risk_factors["PhysicalActivity"_id] = final_value;
}

// MAHIMA: Function to initialise simple physical activity model using log-normal distribution
// This is for India or any project that does not have region, ethncity, income continuous etc
// in the CSV
void StaticLinearModel::initialise_simple_physical_activity(
    RuntimeContext &context, Person &person, Random &random,
    const PhysicalActivityModel &model) const {
    // India approach: Simple model with standard deviation from the model
    double expected = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                   std::nullopt, false);
    double rand = random.next_normal(0.0, model.stddev);
    double factor = expected * exp(rand - (0.5 * pow(model.stddev, 2)));

    // Store in both physical_activity and risk_factors for compatibility
    person.physical_activity = factor;
    person.risk_factors["PhysicalActivity"_id] = factor;
}

// Helper function to create shared_ptr from unique_ptr before moving
template <typename T> static std::shared_ptr<T> create_shared_from_unique(std::unique_ptr<T> &ptr) {
    return ptr ? std::make_shared<T>(*ptr) : nullptr;
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
    std::unique_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors,
    bool is_continuous_income_model, const LinearModelParams &continuous_income_model,
    const std::string &income_categories,
    std::unordered_map<core::Identifier, PhysicalActivityModel> physical_activity_models,
    bool has_active_policies)
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    : RiskFactorAdjustableModelDefinition{std::move(expected), std::move(expected_trend),
                                          std::move(trend_steps), trend_type},
      expected_trend_{create_shared_from_unique(expected_trend)},
      expected_trend_boxcox_{create_shared_from_unique(expected_trend_boxcox)},
      trend_models_{create_shared_from_unique(trend_models)},
      trend_ranges_{create_shared_from_unique(trend_ranges)},
      trend_lambda_{create_shared_from_unique(trend_lambda)},
      // Income trend member variables
      trend_type_{trend_type},
      expected_income_trend_{expected_income_trend
                                 ? std::make_shared<std::unordered_map<core::Identifier, double>>(
                                       std::move(*expected_income_trend))
                                 : nullptr},
      expected_income_trend_boxcox_{
          expected_income_trend_boxcox
              ? std::make_shared<std::unordered_map<core::Identifier, double>>(
                    std::move(*expected_income_trend_boxcox))
              : nullptr},
      income_trend_steps_{income_trend_steps
                              ? std::make_shared<std::unordered_map<core::Identifier, int>>(
                                    std::move(*income_trend_steps))
                              : nullptr},
      income_trend_models_{income_trend_models ? std::make_shared<std::vector<LinearModelParams>>(
                                                     std::move(*income_trend_models))
                                               : nullptr},
      income_trend_ranges_{
          income_trend_ranges
              ? std::make_shared<std::vector<core::DoubleInterval>>(std::move(*income_trend_ranges))
              : nullptr},
      income_trend_lambda_{income_trend_lambda ? std::make_shared<std::vector<double>>(
                                                     std::move(*income_trend_lambda))
                                               : nullptr},
      income_trend_decay_factors_{
          income_trend_decay_factors
              ? std::make_shared<std::unordered_map<core::Identifier, double>>(
                    std::move(*income_trend_decay_factors))
              : nullptr},
      // Common member variables
      names_{std::move(names)}, models_{std::move(models)}, ranges_{std::move(ranges)},
      lambda_{std::move(lambda)}, stddev_{std::move(stddev)}, cholesky_{std::move(cholesky)},
      policy_models_{std::move(policy_models)}, policy_ranges_{std::move(policy_ranges)},
      policy_cholesky_{std::move(policy_cholesky)}, info_speed_{info_speed},
      rural_prevalence_{std::move(rural_prevalence)}, income_models_{std::move(income_models)},
      physical_activity_stddev_{physical_activity_stddev},
      physical_activity_models_{physical_activity_models},
      has_physical_activity_models_{!physical_activity_models.empty()},
      // Continuous income model support (FINCH approach)
      is_continuous_income_model_{is_continuous_income_model},
      continuous_income_model_{continuous_income_model}, income_categories_{income_categories},
      has_active_policies_{has_active_policies} {

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
    if (trend_type_ == hgps::TrendType::Trend) {
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
    if (trend_type_ == hgps::TrendType::IncomeTrend) {
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
    }

    // Validate income trend data consistency
    if (income_trend_models_ && income_trend_models_->empty()) {
        throw core::HgpsException("Income trend model list is empty");
    }
    if (income_trend_ranges_ && income_trend_ranges_->empty()) {
        throw core::HgpsException("Income trend ranges list is empty");
    }
    if (income_trend_lambda_ && income_trend_lambda_->empty()) {
        throw core::HgpsException("Income trend lambda list is empty");
    }

    // Validate that all risk factors have income trend parameters
    if (trend_type_ == hgps::TrendType::IncomeTrend && expected_income_trend_) {
        for (const auto &name : names_) {
            if (!expected_income_trend_->contains(name)) {
                throw core::HgpsException(
                    "One or more expected income trend value is missing for risk factor: " +
                    name.to_string());
            }
            if (!expected_income_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected income trend BoxCox value is "
                                          "missing for risk factor: " +
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

    // Policy detection is now done earlier in the model parser for better performance

    // Validate regular trend parameters for all risk factors only if trend type is Trend
    if (trend_type_ == hgps::TrendType::Trend) {
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
        income_trend_ranges_, income_trend_lambda_, income_trend_decay_factors_,
        is_continuous_income_model_, continuous_income_model_, income_categories_,
        physical_activity_models_, has_active_policies_);
}

} // namespace hgps
