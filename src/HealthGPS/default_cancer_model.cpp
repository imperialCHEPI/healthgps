#include "default_cancer_model.h"
#include "person.h"
#include "runtime_context.h"

#include <oneapi/tbb/parallel_for_each.h>
#include <iostream>

namespace hgps {

DefaultCancerModel::DefaultCancerModel(DiseaseDefinition &definition, WeightModel &&classifier,
                                       const core::IntegerInterval &age_range)
    : definition_{definition}, weight_classifier_{std::move(classifier)},
      average_relative_risk_{create_age_gender_table<double>(age_range)} {
    if (definition_.get().identifier().group != core::DiseaseGroup::cancer) {
        throw std::invalid_argument("Disease definition group mismatch, must be 'cancer'.");
    }
}

core::DiseaseGroup DefaultCancerModel::group() const noexcept { return core::DiseaseGroup::cancer; }

const core::Identifier &DefaultCancerModel::disease_type() const noexcept {
    return definition_.get().identifier().code;
}

void DefaultCancerModel::initialise_disease_status(RuntimeContext &context) {
    int prevalence_id = definition_.get().table().at(MeasureKey::prevalence);

    auto relative_risk_table = calculate_average_relative_risk(context);
    for (auto &person : context.population()) {
        if (!person.is_active() || !definition_.get().table().contains(person.age)) {
            continue;
        }

        double relative_risk = 1.0;
        relative_risk *= calculate_relative_risk_for_risk_factors(person);

        double average_relative_risk = relative_risk_table(person.age, person.gender);

        double prevalence = definition_.get().table()(person.age, person.gender).at(prevalence_id);
        double probability = prevalence * relative_risk / average_relative_risk;
        double hazard = context.random().next_double();
        if (hazard < probability) {
            int time_since_onset = calculate_time_since_onset(context, person.gender);
            // start_time = 0 means the disease existed before the simulation started.
            person.diseases[disease_type()] = Disease{.status = DiseaseStatus::active,
                                                      .start_time = 0,
                                                      .time_since_onset = time_since_onset};
        }
    }
}

void DefaultCancerModel::initialise_average_relative_risk(RuntimeContext &context) {
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
        relative_risk *= calculate_relative_risk_for_diseases(person);

        auto lock = std::unique_lock{sum_mutex};
        sum(person.age, person.gender) += relative_risk;
        count(person.age, person.gender)++;
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
}

void DefaultCancerModel::update_disease_status(RuntimeContext &context) {
    // Order is very important!
    update_remission_cases(context);
    update_incidence_cases(context);
}

double DefaultCancerModel::get_excess_mortality(const Person &person) const noexcept {
    int mortality_id = definition_.get().table().at(MeasureKey::mortality);

    const auto &disease_info = person.diseases.at(disease_type());
    auto max_onset = definition_.get().parameters().max_time_since_onset;
    if (disease_info.time_since_onset < 0 || disease_info.time_since_onset >= max_onset) {
        return 0.0;
    }

    double excess_mortality = definition_.get().table()(person.age, person.gender).at(mortality_id);
    const auto &sex_death_weights =
        definition_.get().parameters().death_weight.at(disease_info.time_since_onset);
    double death_weight =
        (person.gender == core::Gender::male) ? sex_death_weights.males : sex_death_weights.females;

    return excess_mortality * death_weight;
}

DoubleAgeGenderTable DefaultCancerModel::calculate_average_relative_risk(RuntimeContext &context) {
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

double DefaultCancerModel::calculate_relative_risk_for_risk_factors(const Person &person) const {
    const auto &relative_risk_tables = definition_.get().relative_risk_factors();

    double relative_risk = 1.0;
    for (const auto &[factor_name, factor_value] : person.risk_factors) {
        if (!relative_risk_tables.contains(factor_name)) {
            continue;
        }

        auto factor_value_adjusted = static_cast<float>(
            weight_classifier_.adjust_risk_factor_value(person, factor_name, factor_value));

        const auto &rr_table = relative_risk_tables.at(factor_name);
        relative_risk *= rr_table.at(person.gender)(person.age, factor_value_adjusted);
    }

    return relative_risk;
}

double DefaultCancerModel::calculate_relative_risk_for_diseases(const Person &person) const {
    const auto &relative_risk_tables = definition_.get().relative_risk_diseases();

    double relative_risk = 1.0;
    for (const auto &[disease_name, disease_state] : person.diseases) {
        if (!relative_risk_tables.contains(disease_name)) {
            continue;
        }

        // Only include existing diseases
        if (disease_state.status == DiseaseStatus::active) {
            const auto &rr_table = relative_risk_tables.at(disease_name);
            relative_risk *= rr_table(person.age, person.gender);
        }
    }

    return relative_risk;
}

void DefaultCancerModel::update_remission_cases(RuntimeContext &context) {
    int max_onset = definition_.get().parameters().max_time_since_onset;

    for (auto &person : context.population()) {
        // Skip if person is inactive or newborn.
        if (!person.is_active() || person.age == 0) {
            continue;
        }

        // Skip if person does not have the disease.
        if (!person.diseases.contains(disease_type()) ||
            person.diseases.at(disease_type()).status != DiseaseStatus::active) {
            continue;
        }

        // Increment duration by one year
        auto &disease = person.diseases.at(disease_type());
        disease.time_since_onset++;
        if (disease.time_since_onset >= max_onset) {
            disease.status = DiseaseStatus::free;
            disease.time_since_onset = -1;
        }
    }
}

void DefaultCancerModel::update_incidence_cases(RuntimeContext &context) {
    try {
        // Get the incidence ID using try-catch instead of contains
        int incidence_id;
        try {
            incidence_id = definition_.get().table().at(MeasureKey::incidence);
        } catch (const std::exception& e) {
            std::cerr << "ERROR: No incidence measure found for cancer disease " << disease_type().to_string() << std::endl;
            return;
        }
        
        for (auto &person : context.population()) {
            try {
                // Skip if person is inactive.
                if (!person.is_active()) {
                    continue;
                }

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
                }
                catch (const std::exception& e) {
                    // Handle error and continue with next person
                    std::cerr << "ERROR accessing disease status for person " << person.id() 
                              << ": " << e.what() << std::endl;
                    continue;
                }
                
                if (already_has_active_disease) {
                    continue;
                }

                // Calculate relative risk with safe error handling
                double relative_risk = 1.0;
                try {
                    relative_risk *= calculate_relative_risk_for_risk_factors(person);
                    relative_risk *= calculate_relative_risk_for_diseases(person);
                }
                catch (const std::exception& e) {
                    // If calculation fails, use default relative risk
                    std::cerr << "ERROR calculating relative risk for person " << person.id() 
                              << ": " << e.what() << std::endl;
                    relative_risk = 1.0;
                }

                // Safely get average relative risk with bounds checking
                double average_relative_risk = 1.0;
                try {
                    // Check age and gender ranges before accessing
                    if (average_relative_risk_.contains(person.age, person.gender)) {
                        average_relative_risk = average_relative_risk_.at(person.age, person.gender);
                    } else {
                        // Use a fallback value if not found
                        std::cerr << "WARNING: Missing average_relative_risk for age " << person.age 
                                  << ", gender " << static_cast<int>(person.gender) << std::endl;
                    }
                }
                catch (const std::exception& e) {
                    // If access fails, use default value
                    std::cerr << "ERROR accessing average_relative_risk for person " << person.id() 
                              << ": " << e.what() << std::endl;
                }

                // Safely get incidence rate with bounds checking
                double incidence = 0.0;
                try {
                    // Check if the age exists in the table
                    if (definition_.get().table().contains(person.age)) {
                        // Get the row for the person's age and gender
                        auto table_row = definition_.get().table()(person.age, person.gender);
                        // Use indexing with at() to safely access the incidence rate
                        try {
                            incidence = table_row.at(incidence_id);
                        } catch (const std::out_of_range&) {
                            // Do nothing, leave incidence at 0
                        }
                    }
                }
                catch (const std::exception& e) {
                    // If access fails, use zero incidence
                    std::cerr << "ERROR accessing incidence rate for person " << person.id() 
                              << ": " << e.what() << std::endl;
                }

                // Calculate probability and apply
                double probability = incidence * relative_risk / average_relative_risk;
                
                // Ensure probability is valid
                if (probability < 0.0 || !std::isfinite(probability)) {
                    probability = 0.0;
                } else if (probability > 1.0) {
                    probability = 1.0;
                }
                
                double hazard = context.random().next_double();
                if (hazard < probability) {
                    // Use operator[] instead of at() to avoid exception if key doesn't exist
                    person.diseases[disease_type()] = Disease{.status = DiseaseStatus::active,
                                                            .start_time = context.time_now(),
                                                            .time_since_onset = 0};
                }
            }
            catch (const std::exception& e) {
                // Log error but continue processing other people
                std::cerr << "ERROR processing person " << person.id() << " for cancer incidence: " << e.what() << std::endl;
            }
        }
    }
    catch (const std::exception& e) {
        // Handle any other exceptions
        std::cerr << "ERROR in cancer update_incidence_cases: " << e.what() << std::endl;
    }
}

int DefaultCancerModel::calculate_time_since_onset(RuntimeContext &context,
                                                   const core::Gender &gender) const {

    const auto &pdf = definition_.get().parameters().prevalence_distribution;
    auto values = std::vector<int>{};
    auto cumulative = std::vector<double>{};
    double sum = 0.0;
    for (const auto &item : pdf) {
        double p = (gender == core::Gender::male) ? item.second.males : item.second.females;

        sum += p;
        values.emplace_back(item.first);
        cumulative.emplace_back(sum);
    }

    return context.random().next_empirical_discrete(values, cumulative);
}

} // namespace hgps
