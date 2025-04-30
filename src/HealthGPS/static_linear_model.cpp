#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Input/model_parser.h"
#include "demographic.h"

#include <iostream>
#include <ranges>
#include <utility>

namespace hgps {

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {
    // std::cout << "\nDEBUG: StaticLinearModel::generate_risk_factors - Starting";

    // Verify that all expected risk factors are included in the names_ vector
    verify_risk_factors();

    // NOTE: Demographic variables (region, ethnicity, income, etc.) are already
    // initialized by the DemographicModule in initialise_population

    // Initialise everyone with risk factors.
    for (auto &person : context.population()) {
        initialise_factors(context, person, context.random());
        initialise_physical_activity(context, person, context.random());
    }
    // std::cout << "\nDEBUG: StaticLinearModel::generate_risk_factors - Factors and physical
    // activity completed";

    // Adjust such that risk factor means match expected values.
    adjust_risk_factors(context, names_, ranges_, false);
    // std::cout << "\nDEBUG: StaticLinearModel::generate_risk_factors - Risk factors adjusted";

    // Initialise everyone with policies and trends.
    for (auto &person : context.population()) {
        initialise_policies(person, context.random(), false);
        initialise_trends(context, person);
    }
    // std::cout << "\nDEBUG: StaticLinearModel::generate_risk_factors - Policies and trends
    // initialized";

    // Adjust such that trended risk factor means match trended expected values.
    adjust_risk_factors(context, names_, ranges_, true);
    // std::cout << "\nDEBUG: StaticLinearModel::generate_risk_factors - Trended risk factors
    // adjusted";

    // Print risk factor summary once at the end
    std::string risk_factor_list;
    for (size_t i = 0; i < names_.size(); i++) {
        if (i > 0)
            risk_factor_list += ", ";
        risk_factor_list += names_[i].to_string();
    }
    // std::cout << "\nDEBUG: Successfully completed processing " << names_.size() << " risk
    // factors: " << risk_factor_list;

    // std::cout << "\nDEBUG: StaticLinearModel::generate_risk_factors - Completed";
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {
    /*std::cout
        << "\nDEBUG: StaticLinearModel::update_risk_factors - Starting with population size: "
              << context.population().size() << ", scenario: "
              << (context.scenario().type() == ScenarioType::baseline ? "baseline" : "intervention")
              << ", current time: " << context.time_now();*/

    // HACK: start intervening two years into the simulation.
    bool intervene = (context.scenario().type() == ScenarioType::intervention &&
                      (context.time_now() - context.start_time()) >= 2);

    // Count statistics for summary
    int total_people = 0;
    int newborns = 0;
    int existing = 0;

    // Update risk factors for all people, initializing for newborns.
    // std::cout << "\nDEBUG: StaticLinearModel::update_risk_factors - Beginning to process people";
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        total_people++;

        if (person.age == 0) {
            // For newborns, initialize demographic variables
            newborns++;
            // initialise_sector(person, context.random());
            //  Demographic variables (region, ethnicity, income) are initialized by the
            //  DemographicModule
            initialise_factors(context, person, context.random());
            initialise_physical_activity(context, person, context.random());
        } else {
            // For existing people, only update sector and factors
            existing++;
            // update_sector(person, context.random());
            update_factors(context, person, context.random());
        }
    }
    // std::cout << "\nDEBUG: StaticLinearModel - Processed " << total_people << " people (" <<
    // newborns << " newborns, " << existing << " existing)";

    // Adjust such that risk factor means match expected values.
    // std::cout << "\nDEBUG: StaticLinearModel - Adjusting risk factors";
    adjust_risk_factors(context, names_, ranges_, false);

    // Update policies and trends for all people, initializing for newborns.
    int people_with_policies = 0;
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        people_with_policies++;

        if (person.age == 0) {
            initialise_policies(person, context.random(), intervene);
            initialise_trends(context, person);
        } else {
            update_policies(person, intervene);
            update_trends(context, person);
        }
    }
    // std::cout << "\nDEBUG: StaticLinearModel - Updated policies and trends for "<<
    // people_with_policies << " people";

    // Adjust such that trended risk factor means match trended expected values.
    // std::cout << "\nDEBUG: StaticLinearModel - Adjusting trended risk factors";
    adjust_risk_factors(context, names_, ranges_, true);

    // Apply policies if intervening.
    int people_with_applied_policies = 0;
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        people_with_applied_policies++;
        apply_policies(person, intervene);
    }
    // std::cout << "\nDEBUG: StaticLinearModel::update_risk_factors - Completed";
}

double StaticLinearModel::inverse_box_cox(double factor, double lambda) {
    // Handle extreme lambda values
    if (std::abs(lambda) < 1e-10) {
        // For lambda near zero, use exponential
        double result = std::exp(factor);
        // Check for Inf
        if (!std::isfinite(result)) {
            return 0.0; // Return safe value for Inf
        }
        return result;
    } else {
        // For non-zero lambda
        double base = lambda * factor + 1.0;
        // Check if base is valid for power
        if (base <= 0.0) {
            return 0.0; // Return safe value for negative/zero base
        }

        // Compute power and check result
        double result = std::pow(base, 1.0 / lambda);
        // Validate the result is finite
        if (!std::isfinite(result)) {
            return 0.0; // Return safe value for NaN/Inf
        }

        // Ensure result is non-negative (though power should already guarantee this)
        return std::max(0.0, result);
    }
}

// Calculate the probability of a risk factor being zero using logistic regression- Mahima
double StaticLinearModel::calculate_zero_probability(Person &person,
                                                     size_t risk_factor_index) const {
    // Get the logistic model for this risk factor
    const auto &logistic_model = logistic_models_[risk_factor_index];

    // Calculate the linear predictor using the logistic model
    double logistic_linear_term = logistic_model.intercept;

    // Add the coefficients
    for (const auto &[coef_name, coef_value] : logistic_model.coefficients) {
        logistic_linear_term += coef_value * person.get_risk_factor_value(coef_name);
    }

    // Add residual from the person's residual for this risk factor
    auto residual_name = core::Identifier{names_[risk_factor_index].to_string() + "_residual"};
    if (person.risk_factors.find(residual_name) != person.risk_factors.end()) {
        // Use the existing residual stored for this risk factor
        double residual = person.risk_factors.at(residual_name);
        logistic_linear_term += residual * stddev_[risk_factor_index];
    }

    // logistic function: p = 1 / (1 + exp(-linear_term))
    double probability = 1.0 / (1.0 + std::exp(-logistic_linear_term));

    // Only print if probability is outside the valid range [0,1] coz logistic regression should be
    // only within 0 and 1 This should never happen with a proper logistic function, but check for
    // numerical issues
    if (probability < 0.0 || probability > 1.0) {
        std::cout << "\nWARNING: Invalid logistic probability for "
                  << names_[risk_factor_index].to_string() << ": " << probability
                  << " (should be between 0 and 1)";
    }

    // Print sample probability values for verification
    // Using a hash of the person's ID and risk factor index to create deterministic sampling
    /* static int sample_counter = 0;
    if (++sample_counter % 10000 == 0) {  // Print only 1 in 10000 values to avoid flooding
        std::cout << "\nSAMPLE: Risk factor for LOGISTIC REGRESSION "
                  << names_[risk_factor_index].to_string() << " probability: " << probability
                  << ", logistic linear term: " << logistic_linear_term;
    }*/

    return probability;
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

        // TWO-STAGE RISK FACTOR MODELING APPROACH- Mahima
        // =======================================================================
        // Previous approach: Use BoxCox transformation to calculate risk factor values
        // New approach: First use logistic regression to determine if the risk factor value should
        // be zero
        //              If non-zero, then use BoxCox transformation to calculate the actual value
        // =======================================================================

        // STAGE 1: Determine if risk factor should be zero using logistic regression
        double zero_probability = calculate_zero_probability(person, i);

        // Sample from this probability to determine if risk factor should be zero
        double random_sample = random.next_double(); // Uniform random value between 0 and 1
        if (random_sample < zero_probability) {
            // Risk factor should be zero
            person.risk_factors[names_[i]] = 0.0;
            continue;
        }

        // STAGE 2: Calculate non-zero risk factor value using BoxCox transformation
        // Using the original model logic from before
        double factor = linear[i] + residual * stddev_[i];
        factor = expected * inverse_box_cox(factor, lambda_[i]);
        factor = ranges_[i].clamp(factor);

        // Save risk factor
        person.risk_factors[names_[i]] = factor;

        // Print sample risk factor values for verification
        /*static int factor_sample_counter = 0;
        if (++factor_sample_counter % 2000 == 0) {  // Even less frequent to avoid flooding
            std::cout << "\nRISK FACTOR VALUE IN INITIALISE FACTORS: " << names_[i].to_string()
                      << " = " << factor
                      << " (Person ID: " << person.id()
                      << ", Age: " << person.age
                      << ", Gender: " << (person.gender == core::Gender::male ? "M" : "F") << ")";
        }*/
    }
}

void StaticLinearModel::update_factors(RuntimeContext &context, Person &person,
                                       Random &random) const {

    // Correlated residual sampling.
    auto residuals = compute_residuals(random, cholesky_);

    // Approximate risk factors with linear models.
    auto linear = compute_linear_models(person, models_);

    // Update residuals and risk factors (should exist).
    for (size_t RiskFactorIndex = 0; RiskFactorIndex < names_.size(); RiskFactorIndex++) {

        // Update residual.
        auto residual_name = core::Identifier{names_[RiskFactorIndex].to_string() + "_residual"};
        double residual_old = person.risk_factors.at(residual_name);
        double residual = residuals[RiskFactorIndex] * info_speed_;
        residual += sqrt(1.0 - info_speed_ * info_speed_) * residual_old;

        // Save residual.
        person.risk_factors.at(residual_name) = residual;

        // Update risk factor.
        double expected = get_expected(context, person.gender, person.age, names_[RiskFactorIndex],
                                       ranges_[RiskFactorIndex], false);

        // TWO-STAGE RISK FACTOR MODELING APPROACH- Mahima
        // =======================================================================
        // Previous approach: Use BoxCox transformation to calculate risk factor values
        // New approach: First use logistic regression to determine if the risk factor value should
        // be zero
        //              If non-zero, then use BoxCox transformation to calculate the actual value
        // =======================================================================

        // STAGE 1: Determine if risk factor should be zero using logistic regression
        double zero_probability = calculate_zero_probability(person, RiskFactorIndex);

        // Sample from this probability to determine if risk factor should be zero
        double random_sample = random.next_double(); // Uniform random value between 0 and 1
        if (random_sample < zero_probability) {
            // Risk factor should be zero
            person.risk_factors.at(names_[RiskFactorIndex]) = 0.0;
            continue; // Skip to next risk factor
        }

        // STAGE 2: Calculate non-zero risk factor value using BoxCox transformation
        // Using the original model logic from before
        double factor = linear[RiskFactorIndex] + residual * stddev_[RiskFactorIndex];
        factor = expected * inverse_box_cox(factor, lambda_[RiskFactorIndex]);
        factor = ranges_[RiskFactorIndex].clamp(factor);

        // Save risk factor
        person.risk_factors.at(names_[RiskFactorIndex]) = factor;

        // Occasionally print sample risk factor values for verification
        /* static int factor_sample_counter = 0;
        if (++factor_sample_counter % 2000 == 0) {  // Even less frequent to avoid flooding
            std::cout << "\nRISK FACTOR VALUE IN UPDATE FACTORS: " <<
        names_[RiskFactorIndex].to_string()
                      << " = " << factor
                      << " (Person ID: " << person.id()
                      << ", Age: " << person.age
                      << ", Gender: " << (person.gender == core::Gender::male ? "M" : "F") << ")";
        }*/
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

        // Ensure the risk factor exists (might be zero from two-stage model)
        if (person.risk_factors.find(names_[i]) == person.risk_factors.end()) {
            std::cout << "\nDEBUG: Risk factor " << names_[i].to_string()
                      << " not found, initializing to zero";
            person.risk_factors[names_[i]] = 0.0;
        }
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
        if (i >= models.size()) {
            std::cout << "\nERROR: compute_linear_models - Index " << i
                      << " is out of bounds for models vector of size " << models.size();
            linear.push_back(0.0); // Default value
            continue;
        }

        auto name = names_[i];
        auto model = models[i];
        double factor = model.intercept;

        for (const auto &[coefficient_name, coefficient_value] : model.coefficients) {
            // Skip the standard deviation entry as it's not a factor
            if (coefficient_name == "StandardDeviation"_id)
                continue;

            double value = person.get_risk_factor_value(coefficient_name);
            factor += coefficient_value * value;
        }

        for (const auto &[coefficient_name, coefficient_value] : model.log_coefficients) {
            double value = person.get_risk_factor_value(coefficient_name);

            if (value <= 0) {
                value = 1e-10; // Avoid log of zero or negative
            }

            factor += coefficient_value * log(value);
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
// Physical activity depends on age. gender, region, ethnicity, income_continuous and random noise
// with std dev Loaded from the static_model.json under the Physical Activity Models section
void StaticLinearModel::initialise_physical_activity([[maybe_unused]] RuntimeContext &context,
                                                     Person &person, Random &random) const {
    // std::cout << "\nDEBUG: Inside physical activity initialization";

    // Check if we have any models before proceeding
    if (physical_activity_models_.empty()) {
        std::cout
            << "\nERROR: physical_activity_models_ is empty! Cannot initialize physical activity.";
        std::cout << "\nERROR: Please check that PhysicalActivityModels are properly defined in "
                     "the configuration.";
        // Don't set a default value - let the error surface so it can be diagnosed
        return;
    }

    // Continue only if models are available
    double final_value = 0.0;

    // Look for the continuous model first
    auto model_it = physical_activity_models_.find(core::Identifier("continuous"));

    // If continuous model not found, use the first available model
    if (model_it == physical_activity_models_.end()) {
        std::cout << "\nERROR: No 'continuous' model found, using first available model";
        model_it = physical_activity_models_.begin();
    }

    // Check again that we have a valid iterator
    if (model_it != physical_activity_models_.end()) {
        const auto &model = model_it->second;

        // Start with the intercept
        double value = model.intercept;
        // std::cout << "\nDEBUG: Using physical activity intercept value: " << value;

        // Process coefficients
        for (const auto &[factor_name, coefficient] : model.coefficients) {
            // Skip the standard deviation entry as it's not a factor
            if (factor_name == "StandardDeviation"_id)
                continue;

            // Apply coefficient based on its name
            double factor_value = 0.0;

            // Age effects
            if (factor_name == "Age"_id) {
                factor_value = static_cast<double>(person.age);
            } else if (factor_name == "Age2"_id) {
                factor_value = std::pow(person.age, 2);
            } else if (factor_name == "Age3"_id) {
                factor_value = std::pow(person.age, 3);
            }
            // Gender effect
            else if (factor_name == "Gender"_id) {
                factor_value = person.gender_to_value();
            }
            // Sector effect
            else if (factor_name == "Sector"_id) {
                factor_value = person.sector_to_value();
            }
            // Region effects
            else if (factor_name == "England"_id && person.region == core::Region::England) {
                factor_value = 1.0;
            } else if (factor_name == "Wales"_id && person.region == core::Region::Wales) {
                factor_value = 1.0;
            } else if (factor_name == "Scotland"_id && person.region == core::Region::Scotland) {
                factor_value = 1.0;
            } else if (factor_name == "NorthernIreland"_id &&
                       person.region == core::Region::NorthernIreland) {
                factor_value = 1.0;
            }
            // Ethnicity effects
            else if (factor_name == "White"_id && person.ethnicity == core::Ethnicity::White) {
                factor_value = 1.0;
            } else if (factor_name == "Black"_id && person.ethnicity == core::Ethnicity::Black) {
                factor_value = 1.0;
            } else if (factor_name == "Asian"_id && person.ethnicity == core::Ethnicity::Asian) {
                factor_value = 1.0;
            } else if (factor_name == "Mixed"_id && person.ethnicity == core::Ethnicity::Mixed) {
                factor_value = 1.0;
            } else if ((factor_name == "Others"_id || factor_name == "Other"_id) &&
                       person.ethnicity == core::Ethnicity::Other) {
                factor_value = 1.0;
            }
            // Income continuous value
            else if (factor_name == "Income"_id) {
                factor_value = person.income_continuous;
            }
            // If we already have this factor, use its value
            else if (person.risk_factors.count(factor_name) > 0) {
                factor_value = person.risk_factors.at(factor_name);
            }

            // Add to our value
            value += coefficient * factor_value;
        }

        // Get the standard deviation
        double pa_stddev = physical_activity_stddev_; // Default
        if (model.coefficients.count("StandardDeviation"_id) > 0) {
            pa_stddev = model.coefficients.at("StandardDeviation"_id);
        }

        // Add random noise using the standard deviation
        double noise = random.next_normal(0.0, pa_stddev);

        // Calculate final value with noise
        final_value = value * (1.0 + noise);
        // std::cout << "\nDEBUG: Calculated final physical activity value: " << final_value;

        // Set the physical activity value
        person.risk_factors[core::Identifier("PhysicalActivity")] = final_value;
        person.physical_activity = final_value;
    } else {
        std::cout << "\nERROR: No valid physical activity models found!";
        // Don't set a default value - let the error surface
        return;
    }

    // std::cout << "\nDEBUG: Finished physical activity initialization for person ID " <<
    // person.id();
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
    const std::unordered_map<core::Identifier, LinearModelParams> &physical_activity_models,
    std::vector<LinearModelParams> logistic_models)
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
      physical_activity_models_{physical_activity_models},
      logistic_models_{std::move(logistic_models)} {

    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (ranges_.empty()) {
        throw core::HgpsException("Risk factor ranges list is empty");
    }

    // Print the loaded ranges for verification
    std::cout << "\n======= LOADED RISK FACTOR RANGES =======";
    for (size_t i = 0; i < names_.size() && i < ranges_.size(); ++i) {
        std::cout << "\nRisk factor: " << names_[i].to_string() << ", Range: ["
                  << ranges_[i].lower() << " , " << ranges_[i].upper() << "]";
    }
    std::cout << "\n=========================================\n";

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

    // If logistic models are empty, initialize them to be the same as the boxcox models
    // This is just a fallback in case logistic models were not provided
    if (logistic_models_.empty()) {
        std::cout
            << "\nWARNING: No logistic regression models provided, using BoxCox models as fallback";
        logistic_models_ = models_;
    }

    // Check if the number of logistic models matches the number of risk factors
    if (logistic_models_.size() != names_.size()) {
        throw core::HgpsException(
            fmt::format("Number of logistic models ({}) does not match number of risk factors ({})",
                        logistic_models_.size(), names_.size()));
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    // Debug information about physical_activity_models_
    // std::cout << "\nDEBUG: In create_model - physical_activity_models_ size: " <<
    // physical_activity_models_.size() << std::endl;

    /*if (!physical_activity_models_.empty()) {
        std::cout << "\nDEBUG: First model key: "
                  << physical_activity_models_.begin()->first.to_string() << std::endl;
    }*/

    return std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_, names_, models_, ranges_,
        lambda_, stddev_, cholesky_, policy_models_, policy_ranges_, policy_cholesky_,
        trend_models_, trend_ranges_, trend_lambda_, info_speed_, rural_prevalence_,
        region_prevalence_, ethnicity_prevalence_, income_models_, region_models_,
        physical_activity_stddev_, get_physical_activity_models(), logistic_models_);
}

// Add a new method to verify risk factors
void StaticLinearModel::verify_risk_factors() const {
    // std::cout << "\nDEBUG: StaticLinearModel::verify_risk_factors - Verifying risk factor
    // configuration";

    // Check model configuration consistency - no hardcoded values
    if (names_.size() != models_.size()) {
        std::cout << "\nWARNING: Mismatch between names_ size (" << names_.size()
                  << ") and models_ size (" << models_.size() << ")";
    }

    if (names_.size() != ranges_.size()) {
        std::cout << "\nWARNING: Mismatch between names_ size (" << names_.size()
                  << ") and ranges_ size (" << ranges_.size() << ")";
    }

    if (names_.size() != lambda_.size()) {
        std::cout << "\nWARNING: Mismatch between names_ size (" << names_.size()
                  << ") and lambda_ size (" << lambda_.size() << ")";
    }

    if (names_.size() != stddev_.size()) {
        std::cout << "\nWARNING: Mismatch between names_ size (" << names_.size()
                  << ") and stddev_ size (" << stddev_.size() << ")";
    }

    // Check RiskFactorModels from expected_trend_boxcox_ map against names_
    if (expected_trend_boxcox_) {
        for (const auto &name : names_) {
            if (!expected_trend_boxcox_->contains(name)) {
                std::cout << "\nWARNING: Risk factor " << name.to_string()
                          << " not found in expected_trend_boxcox_ map";
            }
        }
    }

    // std::cout << "\nDEBUG: StaticLinearModel::verify_risk_factors - Verification completed with "
    // << names_.size() << " risk factors";
}

} // namespace hgps
