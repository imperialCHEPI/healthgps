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
    const std::unordered_map<core::Identifier, PhysicalActivityModel> &physical_activity_models)
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
      has_physical_activity_models_{!physical_activity_models.empty()} {

    std::cout << "\nDEBUG: StaticLinearModel constructor - member variables initialized";
    std::cout << "\nDEBUG: is_continuous_income_model_ = " << (is_continuous_income_model_ ? "true" : "false");
    std::cout << "\nDEBUG: continuous_income_model_.intercept = " << continuous_income_model_.intercept;
    std::cout << "\nDEBUG: continuous_income_model_.coefficients.size() = " << continuous_income_model_.coefficients.size();
    std::cout << "\nDEBUG: About to check member variables...";
    
    std::cout << "\nDEBUG: Checking names_...";
    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    std::cout << " OK, names_.size() = " << names_.size();
    
    std::cout << "\nDEBUG: Checking models_...";
    if (models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    std::cout << " OK, models_.size() = " << models_.size();
    
    std::cout << "\nDEBUG: Checking ranges_...";
    if (ranges_.empty()) {
        throw core::HgpsException("Risk factor ranges list is empty");
    }
    std::cout << " OK, ranges_.size() = " << ranges_.size();
    
    std::cout << "\nDEBUG: Checking lambda_...";
    if (lambda_.empty()) {
        throw core::HgpsException("Risk factor lambda list is empty");
    }
    std::cout << " OK, lambda_.size() = " << lambda_.size();
    
    std::cout << "\nDEBUG: Checking stddev_...";
    if (stddev_.empty()) {
        throw core::HgpsException("Risk factor standard deviation list is empty");
    }
    std::cout << " OK, stddev_.size() = " << stddev_.size();
    
    std::cout << "\nDEBUG: Checking cholesky_...";
    if (!cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    std::cout << " OK, cholesky_.rows() = " << cholesky_.rows() << ", cholesky_.cols() = " << cholesky_.cols();
    
    std::cout << "\nDEBUG: Checking policy_models_...";
    if (policy_models_.empty()) {
        throw core::HgpsException("Intervention policy model list is empty");
    }
    std::cout << " OK, policy_models_.size() = " << policy_models_.size();
    
    std::cout << "\nDEBUG: Checking policy_ranges_...";
    if (policy_ranges_.empty()) {
        throw core::HgpsException("Intervention policy ranges list is empty");
    }
    std::cout << " OK, policy_ranges_.size() = " << policy_ranges_.size();
    
    std::cout << "\nDEBUG: Checking policy_cholesky_...";
    if (!policy_cholesky_.allFinite()) {
        throw core::HgpsException("Intervention policy Cholesky matrix contains non-finite values");
    }
    std::cout << " OK, policy_cholesky_.rows() = " << policy_cholesky_.rows() << ", policy_cholesky_.cols() = " << policy_cholesky_.cols();
    // Validate regular trend parameters only if trend type is Trend
    std::cout << "\nDEBUG: Checking trend validation...";
    std::cout << "\nDEBUG: trend_type_ = " << static_cast<int>(trend_type_);
    if (trend_type_ == TrendType::Trend) {
        std::cout << "\nDEBUG: Validating regular trend parameters...";
        std::cout << "\nDEBUG: Checking trend_models_...";
        if (trend_models_->empty()) {
            throw core::HgpsException("Time trend model list is empty");
        }
        std::cout << " OK, trend_models_.size() = " << trend_models_->size();
        
        std::cout << "\nDEBUG: Checking trend_ranges_...";
        if (trend_ranges_->empty()) {
            throw core::HgpsException("Time trend ranges list is empty");
        }
        std::cout << " OK, trend_ranges_.size() = " << trend_ranges_->size();
        
        std::cout << "\nDEBUG: Checking trend_lambda_...";
        if (trend_lambda_->empty()) {
            throw core::HgpsException("Time trend lambda list is empty");
        }
        std::cout << " OK, trend_lambda_.size() = " << trend_lambda_->size();
    } else {
        std::cout << "\nDEBUG: Skipping regular trend validation (trend_type_ != Trend)";
    }

    // Validate income trend parameters if income trend is enabled
    std::cout << "\nDEBUG: Checking income trend validation...";
    if (trend_type_ == TrendType::IncomeTrend) {
        std::cout << "\nDEBUG: Validating income trend parameters...";
        std::cout << "\nDEBUG: Checking expected_income_trend_...";
        if (!expected_income_trend_) {
            throw core::HgpsException(
                "Income trend is enabled but expected_income_trend is missing");
        }
        std::cout << " OK";
        
        std::cout << "\nDEBUG: Checking expected_income_trend_boxcox_...";
        if (!expected_income_trend_boxcox_) {
            throw core::HgpsException(
                "Income trend is enabled but expected_income_trend_boxcox is missing");
        }
        std::cout << " OK";
        
        std::cout << "\nDEBUG: Checking income_trend_steps_...";
        if (!income_trend_steps_) {
            throw core::HgpsException("Income trend is enabled but income_trend_steps is missing");
        }
        std::cout << " OK";
        
        std::cout << "\nDEBUG: Checking income_trend_models_...";
        if (!income_trend_models_) {
            throw core::HgpsException("Income trend is enabled but income_trend_models is missing");
        }
        std::cout << " OK";
        
        std::cout << "\nDEBUG: Checking income_trend_ranges_...";
        if (!income_trend_ranges_) {
            throw core::HgpsException("Income trend is enabled but income_trend_ranges is missing");
        }
        std::cout << " OK";
        
        std::cout << "\nDEBUG: Checking income_trend_lambda_...";
        if (!income_trend_lambda_) {
            throw core::HgpsException("Income trend is enabled but income_trend_lambda is missing");
        }
        std::cout << " OK";
        
        std::cout << "\nDEBUG: Checking income_trend_decay_factors_...";
        if (!income_trend_decay_factors_) {
            throw core::HgpsException(
                "Income trend is enabled but income_trend_decay_factors is missing");
        }
        std::cout << " OK";
    } else {
        std::cout << "\nDEBUG: Skipping income trend validation (trend_type_ != IncomeTrend)";
    }

    // Validate income trend data consistency
    std::cout << "\nDEBUG: Checking income trend data consistency...";
    if (income_trend_models_ && income_trend_models_->empty()) {
        throw core::HgpsException("Income trend model list is empty");
    }
    std::cout << " OK";
    
    if (income_trend_ranges_ && income_trend_ranges_->empty()) {
        throw core::HgpsException("Income trend ranges list is empty");
    }
    std::cout << " OK";
    
    if (income_trend_lambda_ && income_trend_lambda_->empty()) {
        throw core::HgpsException("Income trend lambda list is empty");
    }
    std::cout << " OK";

    // Validate that all risk factors have income trend parameters
    std::cout << "\nDEBUG: Checking risk factor income trend parameters...";
    // Only validate income trend parameters if income trend is enabled
    if (trend_type_ == TrendType::IncomeTrend && expected_income_trend_) {
        std::cout << "\nDEBUG: Validating income trend parameters for all risk factors...";
        for (const auto &name : names_) {
            std::cout << "\nDEBUG: Checking risk factor: " << name.to_string();
            if (!expected_income_trend_->contains(name)) {
                throw core::HgpsException(
                    "One or more expected income trend value is missing for risk factor: " +
                    name.to_string());
            }
            std::cout << " expected_income_trend OK";
            
            if (!expected_income_trend_boxcox_->contains(name)) {
                throw core::HgpsException(
                    "One or more expected income trend BoxCox value is missing for risk factor: " +
                    name.to_string());
            }
            std::cout << " expected_income_trend_boxcox OK";
            
            if (!income_trend_steps_->contains(name)) {
                throw core::HgpsException(
                    "One or more income trend steps value is missing for risk factor: " +
                    name.to_string());
            }
            std::cout << " income_trend_steps OK";
            
            if (!income_trend_decay_factors_->contains(name)) {
                throw core::HgpsException(
                    "One or more income trend decay factor is missing for risk factor: " +
                    name.to_string());
            }
            std::cout << " income_trend_decay_factors OK";
        }
        std::cout << "\nDEBUG: All risk factor income trend parameters validated successfully";
    } else {
        std::cout << "\nDEBUG: Skipping risk factor income trend parameter validation (trend_type_ != IncomeTrend or expected_income_trend_ is null)";
    }

    std::cout << "\nDEBUG: Checking final validation...";
    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    std::cout << " rural_prevalence OK";
    
    if (income_models_.empty()) {
        throw core::HgpsException("Income models mapping is empty");
    }
    std::cout << " income_models OK";

    // Validate regular trend parameters for all risk factors only if trend type is Trend
    std::cout << "\nDEBUG: Checking final trend validation...";
    if (trend_type_ == TrendType::Trend) {
        std::cout << "\nDEBUG: Validating final regular trend parameters...";
        for (const auto &name : names_) {
            std::cout << "\nDEBUG: Checking final trend for risk factor: " << name.to_string();
            if (!expected_trend_->contains(name)) {
                throw core::HgpsException("One or more expected trend value is missing");
            }
            std::cout << " expected_trend OK";
            
            if (!expected_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected trend BoxCox value is missing");
            }
            std::cout << " expected_trend_boxcox OK";
        }
    } else {
        std::cout << "\nDEBUG: Skipping final trend validation (trend_type_ != Trend)";
    }
    
    std::cout << "\nDEBUG: StaticLinearModel constructor completed successfully";
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {
    std::cout << "\nDEBUG: generate_risk_factors called - START";
    std::cout << "\nDEBUG: Population size: " << context.population().size();
    std::cout << "\nDEBUG: Trend type: " << static_cast<int>(trend_type_);
    std::cout << "\nDEBUG: Is continuous income model: "
              << (is_continuous_income_model_ ? "true" : "false");

    // MAHIMA: Initialise everyone. Order is important here.
    // STEP 1: Age and gender initalized in demographic.cpp
    // STEP 2: Region and ethnicity in demographic.cpp (if available)
    // STEP 3: Sector and income in static_linear_model.cpp (below)
    // STEP 4: Risk factors in static_linear_model.cpp (below)
    // STEP 5: Risk factors adjusted to expected means in static_linear_model.cpp (below)
    // STEP 6: Policies in static_linear_model.cpp (below)
    // STEP 7: Trends in static_linear_model.cpp (below) depnds on whether trends are enabled
    // STEP 8: Risk factors adjusted to trended expected means in static_linear_model.cpp (below)
    std::cout << "\nDEBUG: Starting person initialization loop for " << context.population().size()
              << " people...";
    size_t person_count = 0;
    for (auto &person : context.population()) {
        std::cout << "\nDEBUG: Initializing person " << person_count << "...";

        std::cout << "\n  DEBUG: Calling initialise_sector...";
        initialise_sector(person, context.random());
        std::cout << " OK";

        std::cout << "\n  DEBUG: Calling initialise_income...";
        initialise_income(context, person, context.random());
        std::cout << " OK";

        std::cout << "\n  DEBUG: Calling initialise_factors...";
        initialise_factors(context, person, context.random());
        std::cout << " OK";

        std::cout << "\n  DEBUG: Calling initialise_physical_activity...";
        initialise_physical_activity(context, person, context.random());
        std::cout << " OK";

        person_count++;
        if (person_count % 100 == 0) {
            std::cout << "\nDEBUG: Processed " << person_count << " people so far...";
        }
    }
    std::cout << "\nDEBUG: Person initialization loop completed successfully for " << person_count
              << " people";

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
            initialise_UPF_trends(context, person);
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
            initialise_policies(person, context.random(), intervene);

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
            update_policies(person, intervene);

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
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        apply_policies(person, intervene);
    }
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

void StaticLinearModel::initialise_income(RuntimeContext &context, Person &person, Random &random) {
    std::cout << "\n    DEBUG: initialise_income called for person age=" << person.age
              << ", gender=" << (person.gender == core::Gender::male ? "male" : "female");
    if (is_continuous_income_model_) {
        // FINCH approach: Use continuous income calculation
        std::cout << "\n    DEBUG: Using continuous income model (FINCH approach)";
        initialise_continuous_income(context, person, random);
    } else {
        // India approach: Use direct categorical assignment
        std::cout << "\n    DEBUG: Using categorical income model (India approach)";
        initialise_categorical_income(person, random);
    }
    std::cout << "\n    DEBUG: initialise_income completed successfully";
}

void StaticLinearModel::update_income(RuntimeContext &context, Person &person, Random &random) {
    // Only update 18 year olds
    if (person.age == 18) {
        initialise_income(context, person, random);
    }
}

// MAHIMA: This is the India approach for categorical income assignment
// In the static_model.json, we have a set of IncomeModels as categories (e.g., low, medium, high)
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
// In the static_model.json, we have a IncomeModel with intercept, regression values (age, gender,
// region, ethnicity etc) If is_continuous_income_model_ is true, we use this approach- the order
// for this is- calculate continuous income, calculate quartiles, assign category
void StaticLinearModel::initialise_continuous_income(RuntimeContext &context, Person &person,
                                                     Random &random) {
    // FINCH approach: Calculate continuous income, then convert to category
    std::cout << "\n      DEBUG: initialise_continuous_income called";

    // Step 1: Calculate continuous income
    std::cout << "\n      DEBUG: Calling calculate_continuous_income...";
    double continuous_income = calculate_continuous_income(person, random);
    std::cout << " OK, income=" << continuous_income;

    // Store continuous income in risk factors for future use
    std::cout << "\n      DEBUG: Storing continuous income in risk factors...";
    person.risk_factors["income_continuous"_id] = continuous_income;
    std::cout << " OK";

    // Step 2: Convert to income category based on population quartiles
    std::cout << "\n      DEBUG: Calling convert_income_continuous_to_category...";
    person.income =
        convert_income_continuous_to_category(continuous_income, context.population(), random);
    std::cout << " OK, category=" << static_cast<int>(person.income);
}

double StaticLinearModel::calculate_continuous_income(Person &person, Random &random) {
    // Calculate continuous income using the continuous income model
    double income = continuous_income_model_.intercept;

    // Add coefficient contributions for each person attribute
    for (const auto &[factor, coefficient] : continuous_income_model_.coefficients) {
        std::string factor_name = factor.to_string();

        if (factor_name == "IncomeContinuousStdDev") {
            // Skip - this is used for standard deviation, not as a coefficient
            continue;
        }

        if (factor_name == "gender2") {
            // Male = 1, Female = 0
            income += coefficient * (person.gender == core::Gender::male ? 1.0 : 0.0);
        } else if (factor_name == "age1") {
            income += coefficient * person.age;
        } else if (factor_name == "age2") {
            income += coefficient * (person.age * person.age);
        } else if (factor_name == "age3") {
            income += coefficient * (person.age * person.age * person.age);
        } else if (factor_name == "region2") {
            // Region effect - only apply if the region matches
            if (person.region == "region2") {
                income += coefficient;
            }
        } else if (factor_name == "region3") {
            if (person.region == "region3") {
                income += coefficient;
            }
        } else if (factor_name == "region4") {
            if (person.region == "region4") {
                income += coefficient;
            }
        } else if (factor_name == "ethnicity2") {
            // Ethnicity effect - only apply if the ethnicity matches
            if (person.ethnicity == "ethnicity2") {
                income += coefficient;
            }
        } else if (factor_name == "ethnicity3") {
            if (person.ethnicity == "ethnicity3") {
                income += coefficient;
            }
        } else if (factor_name == "ethnicity4") {
            if (person.ethnicity == "ethnicity4") {
                income += coefficient;
            }
        } else if (factor_name == "min" || factor_name == "max") {
            // Skip min/max bounds - they're handled separately
            continue;
        } else {
            // For any other factors, try to get from risk factors
            try {
                income += coefficient * person.get_risk_factor_value(factor);
            } catch (...) {
                // Factor not found, skip it
                std::cout << "Warning: Factor " << factor_name
                          << " not found for continuous income calculation, skipping.\n";
                continue;
            }
        }
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

std::vector<double>
StaticLinearModel::calculate_income_quartiles(const Population &population) const {
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
        } else if (continuous_income <= quartile_thresholds[1]) {
            return core::Income::lowermiddle;
        } else if (continuous_income <= quartile_thresholds[2]) {
            return core::Income::uppermiddle;
        } else {
            return core::Income::high;
        }
    } else {
        // 3-category system: low, middle, high
        // For 3 categories, we'll use the 33rd and 67th percentiles
        if (continuous_income <= quartile_thresholds[0]) {
            return core::Income::low;
        } else if (continuous_income <= quartile_thresholds[2]) {
            return core::Income::middle;
        } else {
            return core::Income::high;
        }
    }
}

void StaticLinearModel::initialise_physical_activity(RuntimeContext &context, Person &person,
                                                     Random &random) const {
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
        // No physical activity models configured - use simple approach with global stddev
        std::cout
            << "\nWARNING: No 'simple' or 'continuous' model found, using simple model as default";
        PhysicalActivityModel simple_model;
        simple_model.model_type = "simple";
        simple_model.stddev = physical_activity_stddev_;
        initialise_simple_physical_activity(context, person, random, simple_model);
    }
}

// MAHIMA: Function to insatialise continuous physical activity model using regression.
// This is for Finch or any project that has region, ethncity, income continuous etc in the CSV
// file.
void StaticLinearModel::initialise_continuous_physical_activity(
    [[maybe_unused]] RuntimeContext &context, Person &person, Random &random,
    const PhysicalActivityModel &model) const {
    // Start with the intercept
    double value = model.intercept;

    // Process coefficients dynamically from CSV file
    for (const auto &[factor_name, coefficient] : model.coefficients) {
        // Skip the standard deviation entry as it's not a factor
        if (factor_name == "stddev"_id) {
            continue;
        }

        // Apply coefficient based on its name - dynamically handle any factor names from CSV
        double factor_value = 0.0;

        // Age effects
        if (factor_name == "age"_id) {
            factor_value = static_cast<double>(person.age);
        } else if (factor_name == "age2"_id) {
            factor_value = std::pow(person.age, 2);
        } else if (factor_name == "age3"_id) {
            factor_value = std::pow(person.age, 3);
        }
        // Gender effect
        else if (factor_name == "gender"_id) {
            factor_value = person.gender_to_value();
        }
        // Sector effect
        else if (factor_name == "sector"_id) {
            factor_value = person.sector_to_value();
        }
        // Region effects - dynamically handle any region names from CSV
        else if (factor_name.to_string() == person.region) {
            factor_value = 1.0;
        }
        // Ethnicity effects - dynamically handle any ethnicity names from CSV
        else if (factor_name.to_string() == person.ethnicity) {
            factor_value = 1.0;
        }
        // Income effects
        else if (factor_name == "income"_id) {
            factor_value = static_cast<double>(person.income);
        } else if (factor_name == "income_continuous"_id) {
            factor_value = person.income_continuous;
        }
        // Risk factor effects
        else {
            // Check if it's a risk factor
            factor_value = person.get_risk_factor_value(factor_name);
        }

        value += coefficient * factor_value;
    }

    // Add random noise using the model's standard deviation
    double rand_noise = random.next_normal(0.0, model.stddev);
    double final_value = value + rand_noise;

    // Apply min/max constraints
    final_value = std::max(model.min_value, std::min(final_value, model.max_value));

    // Set the physical activity value
    person.physical_activity = final_value;
}

// MAHIMA: Function to initialise simple physical activity model using log-normal distribution
// This is for India or any project that does not have region, ethncity, income continuous etc in
// the CSV
void StaticLinearModel::initialise_simple_physical_activity(
    RuntimeContext &context, Person &person, Random &random,
    const PhysicalActivityModel &model) const {
    // India approach: Simple model with standard deviation from the model
    double expected = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                   std::nullopt, false);
    double rand = random.next_normal(0.0, model.stddev);
    double factor = expected * exp(rand - 0.5 * pow(model.stddev, 2));

    // Store in both physical_activity and risk_factors for compatibility
    person.physical_activity = factor;
    person.risk_factors["PhysicalActivity"_id] = factor;
}

// Helper function to create shared_ptr from unique_ptr before moving
template <typename T> std::shared_ptr<T> create_shared_from_unique(std::unique_ptr<T> &ptr) {
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
    std::unordered_map<core::Identifier, PhysicalActivityModel> physical_activity_models)
    // FIXED: Create copies of data before moving unique_ptrs to avoid use-after-move warnings
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
      continuous_income_model_{continuous_income_model}, income_categories_{income_categories} {

    std::cout << "\nDEBUG: StaticLinearModelDefinition constructor - member variables initialized";
    std::cout << "\nDEBUG: is_continuous_income_model_ = "
              << (is_continuous_income_model_ ? "true" : "false");
    std::cout << "\nDEBUG: continuous_income_model_.intercept = "
              << continuous_income_model_.intercept;
    std::cout << "\nDEBUG: continuous_income_model_.coefficients.size() = "
              << continuous_income_model_.coefficients.size();
    std::cout << "\nDEBUG: income_categories_ = " << income_categories_;

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
    std::cout << "\nDEBUG: StaticLinearModelDefinition::create_model called";
    std::cout << "\nDEBUG: Creating StaticLinearModel with continuous_income_model_.intercept = "
              << continuous_income_model_.intercept;
    std::cout << "\nDEBUG: continuous_income_model_.coefficients.size() = "
              << continuous_income_model_.coefficients.size();

    return std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_, names_, models_, ranges_,
        lambda_, stddev_, cholesky_, policy_models_, policy_ranges_, policy_cholesky_,
        trend_models_, trend_ranges_, trend_lambda_, info_speed_, rural_prevalence_, income_models_,
        physical_activity_stddev_, trend_type_, expected_income_trend_,
        expected_income_trend_boxcox_, income_trend_steps_, income_trend_models_,
        income_trend_ranges_, income_trend_lambda_, income_trend_decay_factors_,
        is_continuous_income_model_, continuous_income_model_, income_categories_,
        physical_activity_models_);
}

} // namespace hgps
