#include "default_disease_model.h"
#include "HealthGPS.Core/thread_util.h"
#include "person.h"
#include "runtime_context.h"

namespace hgps {

DefaultDiseaseModel::DefaultDiseaseModel(DiseaseDefinition &definition, WeightModel &&classifier,
                                         const core::IntegerInterval &age_range)
    : definition_{definition}, weight_classifier_{std::move(classifier)},
      average_relative_risk_{create_age_gender_table<double>(age_range)} {
    if (definition_.get().identifier().group == core::DiseaseGroup::cancer) {
        throw std::invalid_argument("Disease definition group mismatch, must not be 'cancer'.");
    }
}

core::DiseaseGroup DefaultDiseaseModel::group() const noexcept {
    return definition_.get().identifier().group;
}

const core::Identifier &DefaultDiseaseModel::disease_type() const noexcept {
    return definition_.get().identifier().code;
}

void DefaultDiseaseModel::initialise_disease_status(RuntimeContext &context) {
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
            person.diseases[disease_type()] =
                Disease{.status = DiseaseStatus::active, .start_time = (context.time_now() - 1)};
        }
    }
}

void DefaultDiseaseModel::initialise_average_relative_risk(RuntimeContext &context) {
    const auto &age_range = context.age_range();
    auto sum = create_age_gender_table<double>(age_range);
    auto count = create_age_gender_table<double>(age_range);
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};
    std::for_each(core::execution_policy, pop.cbegin(), pop.cend(), [&](const auto &person) {
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

void DefaultDiseaseModel::update_disease_status(RuntimeContext &context) {
    // Order is very important!
    update_remission_cases(context);
    update_incidence_cases(context);
}

double DefaultDiseaseModel::get_excess_mortality(const Person &person) const noexcept {
    int mortality_id = definition_.get().table().at(MeasureKey::mortality);

    if (definition_.get().table().contains(person.age)) {
        return definition_.get().table()(person.age, person.gender).at(mortality_id);
    }

    return 0.0;
}

DoubleAgeGenderTable DefaultDiseaseModel::calculate_average_relative_risk(RuntimeContext &context) {
    const auto &age_range = context.age_range();
    auto sum = create_age_gender_table<double>(age_range);
    auto count = create_age_gender_table<double>(age_range);
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};
    std::for_each(core::execution_policy, pop.cbegin(), pop.cend(), [&](const auto &person) {
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
    const auto &relative_risk_tables = definition_.get().relative_risk_factors();

    double relative_risk = 1.0;
    for (const auto &[factor_name, factor_value] : person.risk_factors) {
        if (!relative_risk_tables.contains(factor_name)) {
            continue;
        }

        float factor_value_adjusted = static_cast<float>(
            weight_classifier_.adjust_risk_factor_value(person, factor_name, factor_value));

        const auto &rr_table = relative_risk_tables.at(factor_name);
        relative_risk *= rr_table.at(person.gender)(person.age, factor_value_adjusted);
    }

    return relative_risk;
}

double DefaultDiseaseModel::calculate_relative_risk_for_diseases(const Person &person) const {
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

void DefaultDiseaseModel::update_remission_cases(RuntimeContext &context) {
    int remission_id = definition_.get().table().at(MeasureKey::remission);

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

        auto probability = definition_.get().table()(person.age, person.gender).at(remission_id);
        auto hazard = context.random().next_double();
        if (hazard < probability) {
            person.diseases.at(disease_type()).status = DiseaseStatus::free;
        }
    }
}

void DefaultDiseaseModel::update_incidence_cases(RuntimeContext &context) {
    int incidence_id = definition_.get().table().at(MeasureKey::incidence);

    for (auto &person : context.population()) {
        // Skip if person is inactive.
        if (!person.is_active()) {
            continue;
        }

        // Clear newborn diseases.
        if (person.age == 0) {
            person.diseases.clear();
            continue;
        }

        // Skip if the person already has the disease.
        if (person.diseases.contains(disease_type()) &&
            person.diseases.at(disease_type()).status == DiseaseStatus::active) {
            continue;
        }

        double relative_risk = 1.0;
        relative_risk *= calculate_relative_risk_for_risk_factors(person);
        relative_risk *= calculate_relative_risk_for_diseases(person);

        double average_relative_risk = average_relative_risk_.at(person.age, person.gender);

        double incidence = definition_.get().table()(person.age, person.gender).at(incidence_id);
        double probability = incidence * relative_risk / average_relative_risk;
        double hazard = context.random().next_double();
        if (hazard < probability) {
            person.diseases[disease_type()] =
                Disease{.status = DiseaseStatus::active, .start_time = context.time_now()};
        }
    }
}

} // namespace hgps
