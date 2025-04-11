#include "default_disease_model.h"
#include "person.h"
#include "runtime_context.h"

#include "default_cancer_model.h"
#include <iostream>
#include <oneapi/tbb/parallel_for_each.h>

namespace hgps {

DefaultDiseaseModel::DefaultDiseaseModel(DiseaseDefinition &definition, WeightModel &&classifier,
                                         const core::IntegerInterval &age_range)
    : definition_{definition}, weight_classifier_{std::move(classifier)},
      average_relative_risk_{create_age_gender_table<double>(age_range)} {
    if (definition_.get().identifier().group == core::DiseaseGroup::cancer) {
        throw std::invalid_argument("Disease definition group mismatch, must not be 'cancer'.");
    }
}

core::DiseaseGroup DefaultDiseaseModel::group() const noexcept { return core::DiseaseGroup::other; }

const core::Identifier &DefaultDiseaseModel::disease_type() const noexcept {
    return definition_.get().identifier().code;
}

void DefaultDiseaseModel::initialise_disease_status(RuntimeContext &context) {
    try {
        // Log the start of disease initialization
       // std::cout << "INFO: Initializing disease status for " << disease_type().to_string() << std::endl;

        // Track initialization statistics
        int total_people = 0;
        int active_people = 0;
        int disease_count = 0;
        int error_count = 0;

        // Get prevalence ID with error handling
        int prevalence_id;
        try {
            prevalence_id = definition_.get().table().at(MeasureKey::prevalence);
        } catch (const std::exception &e) {
            std::cerr << "ERROR: Failed to get prevalence measure for "
                      << disease_type().to_string() << ": " << e.what() << std::endl;
            return;
        }

        // Find the highest age in the disease table
        int max_table_age = 0;
        for (int age = 0; age <= 110; age++) {
            if (definition_.get().table().contains(age)) {
                max_table_age = age;
            } else if (age > max_table_age) {
                // Once we find the first missing age after finding valid ages, we can stop
                break;
            }
        }

        if (max_table_age == 0) {
            std::cerr << "CRITICAL ERROR: No age data found in disease table for "
                      << disease_type().to_string() << std::endl;
            return;
        }

        //std::cout << "INFO: Maximum age in disease table for " << disease_type().to_string() << " is " << max_table_age << std::endl;

        auto relative_risk_table = calculate_average_relative_risk(context);
        for (auto &person : context.population()) {
            total_people++;

            try {
                // Skip inactive people
                if (!person.is_active()) {
                    continue;
                }

                active_people++;

                // Handle people with ages above what's in the disease table
                int reference_age = person.age;
                if (reference_age > max_table_age) {
                    // Use the highest available age in the table
                    reference_age = max_table_age;
                }

                // Skip if reference age isn't in the disease table (should not happen now)
                if (!definition_.get().table().contains(reference_age)) {
                    continue;
                }

                // Calculate the relative risk for this person
                double relative_risk = 1.0;
                relative_risk *= calculate_relative_risk_for_risk_factors(person);

                // Get the average relative risk for their demographic
                double average_relative_risk = 1.0; // Default value

                // Try to get the exact age/gender value, but fall back to reference age if needed
                if (relative_risk_table.contains(person.age, person.gender)) {
                    average_relative_risk = relative_risk_table(person.age, person.gender);
                } else if (relative_risk_table.contains(reference_age, person.gender)) {
                    average_relative_risk = relative_risk_table(reference_age, person.gender);
                }

                if (average_relative_risk <= 0.0) {
                    average_relative_risk = 1.0; // Avoid division by zero
                }

                // Get the prevalence rate for their demographic
                double prevalence;
                try {
                    prevalence =
                        definition_.get().table()(reference_age, person.gender).at(prevalence_id);
                } catch (const std::exception &e) {
                    std::cerr << "WARNING: Failed to get prevalence rate for person " << person.id()
                              << " with reference age " << reference_age << " and gender "
                              << static_cast<int>(person.gender) << ": " << e.what() << std::endl;
                    continue;
                }

                // For very old people (above max_table_age), reduce prevalence with age
                if (person.age > max_table_age) {
                    // Apply a decay factor based on how far beyond max_table_age
                    double decay_factor = std::exp(-0.05 * (person.age - max_table_age));
                    prevalence *= decay_factor;
                }

                // Calculate probability with bounds checking
                double probability = prevalence * relative_risk / average_relative_risk;
                if (probability < 0.0 || !std::isfinite(probability)) {
                    probability = 0.0;
                } else if (probability > 1.0) {
                    probability = 1.0;
                }

                // Roll for disease initialization
                double hazard = context.random().next_double();
                if (hazard < probability) {
                    // Initialize disease with status = active and start_time = 0
                    // start_time = 0 means the disease existed before the simulation started
                    person.diseases[disease_type()] =
                        Disease{.status = DiseaseStatus::active, .start_time = 0};
                    disease_count++;
                }
            } catch (const std::exception &e) {
                error_count++;
                std::cerr << "ERROR: Failed to initialize disease status for person " << person.id()
                          << ": " << e.what() << std::endl;
            }
        }

        // Report initialization statistics
        /*std::cout << "INFO: Initialized " << disease_type().to_string() << " for "
                       << disease_count
                  << " out of " << active_people << " active people ("
                  << (disease_count * 100.0 / std::max(1, active_people)) << "%), with "
                  << error_count << " errors" << std::endl;*/ 

        // Warn if no diseases were initialized
        if (disease_count == 0) {
            std::cerr << "WARNING: No " << disease_type().to_string()
                      << " diseases were initialized! This will result in zero prevalence."
                      << std::endl;
        }
    } catch (const std::exception &e) {
        std::cerr << "CRITICAL ERROR: Failed to initialize disease " << disease_type().to_string()
                  << ": " << e.what() << std::endl;
    }
}

void DefaultDiseaseModel::initialise_average_relative_risk(RuntimeContext &context) {
    try {
        const auto &age_range = context.age_range();
        auto sum = create_age_gender_table<double>(age_range);
        auto count = create_age_gender_table<double>(age_range);
        auto &pop = context.population();
        auto sum_mutex = std::mutex{};

        // Find the highest age in the disease table if not already cached
        if (max_table_age_ == 0) {
            for (int age = 0; age <= 110; age++) {
                if (definition_.get().table().contains(age)) {
                    max_table_age_ = age;
                } else if (age > max_table_age_) {
                    break;
                }
            }

            if (max_table_age_ == 0) {
                std::cerr << "CRITICAL ERROR: No age data found in disease table for "
                          << disease_type().to_string() << std::endl;
                return;
            }

            //std::cout << "INFO: Maximum age in disease table for " << disease_type().to_string() << " is " << max_table_age_ << std::endl;
        }

        tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &person) {
            try {
                if (!person.is_active()) {
                    return;
                }

                double relative_risk = 1.0;
                // Use safer calculation with error handling
                try {
                    relative_risk *= calculate_relative_risk_for_risk_factors(person);
                    relative_risk *= calculate_relative_risk_for_diseases(person);
                } catch (const std::exception &e) {
                    // If calculation fails, use default value
                    relative_risk = 1.0;
                }

                auto lock = std::unique_lock{sum_mutex};
                sum(person.age, person.gender) += relative_risk;
                count(person.age, person.gender)++;
            } catch (const std::exception &) {
                // Skip this person if any error occurs
            }
        });

        double default_average = 1.0;
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            auto male_average = default_average;
            auto female_average = default_average;
            if (sum.contains(age)) {
                auto male_count = count(age, core::Gender::male);
                if (male_count > 0.0) {
                    male_average = sum(age, core::Gender::male) / male_count;
                }

                auto female_count = count(age, core::Gender::female);
                if (female_count > 0.0) {
                    female_average = sum(age, core::Gender::female) / female_count;
                }
            }

            average_relative_risk_(age, core::Gender::male) = male_average;
            average_relative_risk_(age, core::Gender::female) = female_average;
        }

        //std::cout << "INFO: Successfully initialized average relative risk for " << disease_type().to_string() << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "ERROR in initialise_average_relative_risk: " << e.what() << std::endl;
    }
}

void DefaultDiseaseModel::update_disease_status(RuntimeContext &context) {
    //std::cout << "DEBUG: Starting disease status updates for " << disease_type().to_string()<< std::endl;

    try {
        // Order is very important!
        //std::cout << "DEBUG: Starting remission updates for " << disease_type().to_string() << std::endl;
        this->update_remission_cases(context);
        //std::cout << "DEBUG: Completed remission updates for " << disease_type().to_string() << std::endl;

        //std::cout << "DEBUG: Starting incidence updates for " << disease_type().to_string() << std::endl;
        this->update_incidence_cases(context);
        //std::cout << "DEBUG: Completed incidence updates for " << disease_type().to_string() << std::endl;

       // std::cout << "DEBUG: Completed disease status updates for " << disease_type().to_string() << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "ERROR in update_disease_status for " << disease_type().to_string() << ": "
                  << e.what() << std::endl;
    } catch (...) {
        std::cerr << "UNKNOWN ERROR in update_disease_status for " << disease_type().to_string()
                  << std::endl;
    }
}

double DefaultDiseaseModel::get_excess_mortality(const Person &person) const noexcept {
    try {
        int mortality_id = definition_.get().table().at(MeasureKey::mortality);

        // Find the highest age in the disease table if not already cached
        if (max_table_age_ == 0) {
            // Use a local variable since the method is const
            int local_max_age = 0;
            for (int age = 0; age <= 110; age++) {
                if (definition_.get().table().contains(age)) {
                    local_max_age = age;
                } else if (age > local_max_age) {
                    break;
                }
            }

            // Use const_cast to update the cache in this const method
            // This is safe because we're caching a computation result
            const_cast<DefaultDiseaseModel *>(this)->max_table_age_ = local_max_age;
        }

        // Handle people with ages above what's in the disease table
        int reference_age = person.age;
        if (reference_age > max_table_age_) {
            // Use the highest available age in the table
            reference_age = max_table_age_;
        }

        // Check if reference age is in the disease table
        if (definition_.get().table().contains(reference_age)) {
            double mortality =
                definition_.get().table()(reference_age, person.gender).at(mortality_id);

            // For very old people (above max_table_age_), reduce mortality with age
            if (person.age > max_table_age_) {
                // Apply a decay factor based on how far beyond max_table_age_
                double decay_factor = std::exp(-0.05 * (person.age - max_table_age_));
                mortality *= decay_factor;
            }

            return mortality;
        }

        return 0.0;
    } catch (...) {
        return 0.0;
    }
}

DoubleAgeGenderTable DefaultDiseaseModel::calculate_average_relative_risk(RuntimeContext &context) {
    const auto &age_range = context.age_range();
    auto sum = create_age_gender_table<double>(age_range);
    auto count = create_age_gender_table<double>(age_range);
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};
    tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &person) {
        if (!person.is_active()) {
            return;
        }

        double relative_risk = 1.0;
        relative_risk *= calculate_relative_risk_for_risk_factors(person);

        auto lock = std::unique_lock{sum_mutex};
        sum(person.age, person.gender) += relative_risk;
        count(person.age, person.gender)++;
    });

    double default_average = 1.0;
    auto result = create_age_gender_table<double>(age_range);
    for (int age = age_range.lower(); age <= age_range.upper(); age++) {
        auto male_average = default_average;
        auto female_average = default_average;
        if (sum.contains(age)) {
            auto male_count = count(age, core::Gender::male);
            if (male_count > 0.0) {
                male_average = sum(age, core::Gender::male) / male_count;
            }

            auto female_count = count(age, core::Gender::female);
            if (female_count > 0.0) {
                female_average = sum(age, core::Gender::female) / female_count;
            }
        }

        result(age, core::Gender::male) = male_average;
        result(age, core::Gender::female) = female_average;
    }

    return result;
}

double DefaultDiseaseModel::calculate_relative_risk_for_risk_factors(const Person &person) const {
    try {
        const auto &relative_risk_tables = definition_.get().relative_risk_factors();

        double relative_risk = 1.0;
        for (const auto &[factor_name, factor_value] : person.risk_factors) {
            if (!relative_risk_tables.contains(factor_name)) {
                continue;
            }

            try {
                auto factor_value_adjusted = static_cast<float>(
                    weight_classifier_.adjust_risk_factor_value(person, factor_name, factor_value));

                const auto &rr_table = relative_risk_tables.at(factor_name);

                // Use the cached max_table_age_ or person.age, whichever is smaller
                int reference_age = person.age;
                if (max_table_age_ > 0 && reference_age > max_table_age_) {
                    reference_age = max_table_age_;
                }

                relative_risk *= rr_table.at(person.gender)(reference_age, factor_value_adjusted);
            } catch (const std::exception &) {
                // If there's any error with this factor, continue with the next one
                continue;
            }
        }

        return relative_risk;
    } catch (const std::exception &) {
        // If there's any error, return default value
        std::cout << "ERROR[default_disease_model]:Going default";
        return 1.0;
    }
}

double DefaultDiseaseModel::calculate_relative_risk_for_diseases(const Person &person) const {
    try {
        const auto &relative_risk_tables = definition_.get().relative_risk_diseases();

        double relative_risk = 1.0;
        for (const auto &[disease_name, disease_state] : person.diseases) {
            // Skip if the disease is not active or if there's no risk table for it
            if (disease_state.status != DiseaseStatus::active ||
                !relative_risk_tables.contains(disease_name)) {
                continue;
            }

            try {
                const auto &rr_table = relative_risk_tables.at(disease_name);

                // Use the cached max_table_age_ or person.age, whichever is smaller
                int reference_age = person.age;
                if (max_table_age_ > 0 && reference_age > max_table_age_) {
                    reference_age = max_table_age_;
                }

                relative_risk *= rr_table(reference_age, person.gender);
            } catch (const std::exception &) {
                // If there's any error accessing the table, continue with the next disease
                continue;
            }
        }

        return relative_risk;
    } catch (const std::exception &) {
        // If there's any error, return default value
        return 1.0;
    }
}

void DefaultDiseaseModel::update_remission_cases(RuntimeContext &context) {
    try {
        int remission_id;
        try {
            remission_id = definition_.get().table().at(MeasureKey::remission);
        } catch (const std::exception &e) {
            std::cerr << "ERROR: Failed to get remission measure for " << disease_type().to_string()
                      << ": " << e.what() << std::endl;
            return;
        }

        // Find the highest age in the disease table if not already cached
        if (max_table_age_ == 0) {
            for (int age = 0; age <= 110; age++) {
                if (definition_.get().table().contains(age)) {
                    max_table_age_ = age;
                } else if (age > max_table_age_) {
                    break;
                }
            }

            if (max_table_age_ == 0) {
                std::cerr << "CRITICAL ERROR: No age data found in disease table for "
                          << disease_type().to_string() << std::endl;
                return;
            }
        }

        for (auto &person : context.population()) {
            try {
                // Skip if person is inactive or newborn.
                if (!person.is_active() || person.age == 0) {
                    continue;
                }

                // Skip if person does not have the disease.
                if (!person.diseases.contains(disease_type()) ||
                    person.diseases.at(disease_type()).status != DiseaseStatus::active) {
                    continue;
                }

                // Handle people with ages above what's in the disease table
                int reference_age = person.age;
                if (reference_age > max_table_age_) {
                    reference_age = max_table_age_;
                }

                // Skip if reference age isn't in the disease table
                if (!definition_.get().table().contains(reference_age)) {
                    continue;
                }

                double probability;
                try {
                    probability =
                        definition_.get().table()(reference_age, person.gender).at(remission_id);

                    // For very old people, adjust remission with age
                    if (person.age > max_table_age_) {
                        // For remission, we might want to increase it slightly with age
                        double adjust_factor = 1.0 + 0.02 * (person.age - max_table_age_);
                        probability *= adjust_factor;
                        // Cap at reasonable value
                        if (probability > 1.0)
                            probability = 1.0;
                    }
                } catch (const std::exception &) {
                    // If no remission data, skip this person
                    continue;
                }

                auto hazard = context.random().next_double();
                if (hazard < probability) {
                    person.diseases.at(disease_type()).status = DiseaseStatus::free;
                }
            } catch (const std::exception &e) {
                // Log error but continue processing other people
                std::cerr << "ERROR processing remission for person: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "ERROR in update_remission_cases: " << e.what() << std::endl;
    }
}

void DefaultDiseaseModel::update_incidence_cases(RuntimeContext &context) {
    try {
        // Track key metrics for reporting
        int total_people = 0;
        int active_people = 0;
        int new_cases = 0;
        int error_count = 0;

        // Get the incidence ID using try-catch instead of contains
        int incidence_id;
        try {
            incidence_id = definition_.get().table().at(MeasureKey::incidence);
        } catch (const std::exception &e) {
            std::cerr << "ERROR: No incidence measure found for disease "
                      << disease_type().to_string() << ": " << e.what() << std::endl;
            return;
        }

        // Find the highest age in the disease table if not already cached
        if (max_table_age_ == 0) {
            for (int age = 0; age <= 110; age++) {
                if (definition_.get().table().contains(age)) {
                    max_table_age_ = age;
                } else if (age > max_table_age_) {
                    break;
                }
            }

            if (max_table_age_ == 0) {

                return;
            }
        }

        for (auto &person : context.population()) {
            total_people++;
            try {
                // Skip if person is inactive.
                if (!person.is_active()) {
                    continue;
                }
                active_people++;

                // Clear newborn diseases.
                if (person.age == 0) {
                    person.diseases.clear();
                    continue;
                }

                // Skip if the person already has the disease - use safer check
                bool already_has_active_disease = false;
                try {
                    if (person.diseases.contains(disease_type())) {
                        if (person.diseases.at(disease_type()).status == DiseaseStatus::active) {
                            already_has_active_disease = true;
                        }
                    }
                } catch (const std::exception &e) {
                    // Handle error and continue with next person
                    std::cerr << "ERROR accessing disease status for person " << person.id() << ": "
                              << e.what() << std::endl;
                    error_count++;
                    continue;
                }

                if (already_has_active_disease) {
                    continue;
                }

                // Handle people with ages above what's in the disease table
                int reference_age = person.age;
                if (reference_age > max_table_age_) {
                    reference_age = max_table_age_;
                }

                // Skip if reference age isn't in the disease table
                if (!definition_.get().table().contains(reference_age)) {
                    continue;
                }

                // Calculate relative risk with safe error handling
                double relative_risk = 1.0;
                try {
                    relative_risk *= calculate_relative_risk_for_risk_factors(person);
                    relative_risk *= calculate_relative_risk_for_diseases(person);
                } catch (const std::exception &) {
                    // If calculation fails, use default relative risk
                    error_count++;
                    relative_risk = 1.0;
                }

                // Safely get average relative risk with bounds checking
                double average_relative_risk = 1.0;
                try {
                    // Check age and gender ranges before accessing
                    if (average_relative_risk_.contains(person.age, person.gender)) {
                        average_relative_risk =
                            average_relative_risk_.at(person.age, person.gender);
                    } else {
                    }
                } catch (const std::exception &) {
                    // If access fails, use default value
                    error_count++;
                }

                // Ensure average_relative_risk is not zero to avoid division by zero
                if (average_relative_risk <= 0.0) {
                    average_relative_risk = 1.0;
                }

                // Safely get incidence rate with bounds checking
                double incidence = 0.0;
                try {
                    // Get the row for the person's age and gender
                    auto table_row = definition_.get().table()(reference_age, person.gender);
                    // Use indexing with at() to safely access the incidence rate
                    incidence = table_row.at(incidence_id);
                } catch (const std::exception &) {
                    // If access fails, continue with zero incidence
                    continue;
                }

                // Skip if incidence is zero (no chance of disease)
                if (incidence <= 0.0) {
                    continue;
                }

                // For very old people (above max_table_age_), reduce incidence with age
                if (person.age > max_table_age_) {
                    // Apply a decay factor based on how far beyond max_table_age_
                    double decay_factor = std::exp(-0.05 * (person.age - max_table_age_));
                    incidence *= decay_factor;
                }

                // Calculate probability and apply
                double probability = incidence * relative_risk / average_relative_risk;

                // Ensure probability is valid
                if (probability < 0.0 || !std::isfinite(probability)) {
                    probability = 0.0;
                } else if (probability > 1.0) {
                    probability = 1.0;
                }

                // Skip if probability is zero (no chance of disease)
                if (probability <= 0.0) {
                    continue;
                }

                double hazard = context.random().next_double();
                if (hazard < probability) {
                    // Person has developed the disease
                    person.diseases[disease_type()] =
                        Disease{.status = DiseaseStatus::active, .start_time = context.time_now()};
                    new_cases++;
                }
            } catch (const std::exception &e) {
                // Log error but continue processing other people
                std::cerr << "ERROR processing person " << person.id()
                          << " for incidence: " << e.what() << std::endl;
                error_count++;
            }
        }

        // Simplified report - only print if there were errors or it's a year divisible by 5
        int year = context.time_now();
        if (error_count > 0 || year % 5 == 0) {
            //std::cout << "INFO: [Year " << year << "] " << disease_type().to_string() << " incidence: " << new_cases << " new cases (" << (new_cases * 100.0 / std::max(1, active_people)) << "%)" << std::endl;
        }

        // Only warn about zero cases if the year is divisible by 5 (to reduce noise)
        if (new_cases == 0 && active_people > 0 && year % 5 == 0) {
            std::cerr << "WARNING: No new " << disease_type().to_string() << " cases in year " << year << std::endl;
        }
    } catch (const std::exception &e) {
        // Handle any other exceptions
        std::cerr << "ERROR in update_incidence_cases: " << e.what() << std::endl;
    }
}

double DefaultDiseaseModel::calculate_disease_probability(const Person &person,
                                                          RuntimeContext &context) {
    try {
        // Get incidence ID with error handling
        int incidence_id;
        try {
            incidence_id = definition_.get().table().at(MeasureKey::incidence);
        } catch (const std::exception &e) {
            std::cerr << "ERROR: Failed to get incidence measure for " << disease_type().to_string()
                      << ": " << e.what() << std::endl;
            return 0.0;
        }

        // Find the highest age in the disease table if not already cached
        if (max_table_age_ == 0) {
            for (int age = 0; age <= 110; age++) {
                if (definition_.get().table().contains(age)) {
                    max_table_age_ = age;
                } else if (age > max_table_age_) {
                    // Once we find the first missing age after finding valid ages, we can stop
                    break;
                }
            }

            if (max_table_age_ == 0) {
                std::cerr << "CRITICAL ERROR: No age data found in disease table for "
                          << disease_type().to_string() << std::endl;
                return 0.0;
            }

            //std::cout << "INFO: Maximum age in disease table for " << disease_type().to_string() << " is " << max_table_age_ << std::endl;
        }

        // Handle people with ages above what's in the disease table
        int reference_age = person.age;
        if (reference_age > max_table_age_) {
            // Use the highest available age in the table
            reference_age = max_table_age_;
        }

        // Skip if reference age isn't in the disease table
        if (!definition_.get().table().contains(reference_age)) {
            return 0.0;
        }

        // Calculate the relative risk for this person
        double relative_risk = 1.0;
        relative_risk *= calculate_relative_risk_for_risk_factors(person);

        // Get the average relative risk for their demographic
        double average_relative_risk = 1.0; // Default value

        // Try to get the exact age/gender value, but fall back to reference age if needed
        if (average_relative_risk_.contains(person.age, person.gender)) {
            average_relative_risk = average_relative_risk_(person.age, person.gender);
        } else if (average_relative_risk_.contains(reference_age, person.gender)) {
            average_relative_risk = average_relative_risk_(reference_age, person.gender);
        }

        if (average_relative_risk <= 0.0) {
            average_relative_risk = 1.0; // Avoid division by zero
        }

        // Get the incidence rate for their demographic
        double incidence;
        try {
            incidence = definition_.get().table()(reference_age, person.gender).at(incidence_id);
        } catch (const std::exception &) {
            return 0.0;
        }

        // For very old people (above max_table_age_), reduce incidence with age
        if (person.age > max_table_age_) {
            // Apply a decay factor based on how far beyond max_table_age_
            double decay_factor = std::exp(-0.05 * (person.age - max_table_age_));
            incidence *= decay_factor;
        }

        // Calculate probability with bounds checking
        double probability = incidence * relative_risk / average_relative_risk;
        if (probability < 0.0 || !std::isfinite(probability)) {
            probability = 0.0;
        } else if (probability > 1.0) {
            probability = 1.0;
        }

        return probability;
    } catch (const std::exception &e) {
        std::cerr << "ERROR: Failed to calculate disease probability for "
                  << disease_type().to_string() << ": " << e.what() << std::endl;
        return 0.0;
    }
}

} // namespace hgps
