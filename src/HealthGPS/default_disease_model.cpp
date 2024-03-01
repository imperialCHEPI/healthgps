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

        auto relative_risk = calculate_relative_risk_for_risk_factors(person);
        auto average_relative_risk = relative_risk_table(person.age, person.gender);
        auto prevalence = definition_.get().table()(person.age, person.gender).at(prevalence_id);
        auto probability = prevalence * relative_risk / average_relative_risk;
        auto hazard = context.random().next_double();
        if (hazard < probability) {
            person.diseases[disease_type()] =
                Disease{.status = DiseaseStatus::active, .start_time = context.time_now()};
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

        auto combine_risk =
            calculate_combined_relative_risk(person, context.start_time(), context.time_now());
        auto lock = std::unique_lock{sum_mutex};
        sum(person.age, person.gender) += combine_risk;
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

        auto relative_risk = calculate_relative_risk_for_risk_factors(person);
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

double DefaultDiseaseModel::calculate_combined_relative_risk(const Person &person, int start_time,
                                                             int time_now) const {
    double relative_risk = 1.0;
    relative_risk *= calculate_relative_risk_for_risk_factors(person);
    relative_risk *= calculate_relative_risk_for_diseases(person, start_time, time_now);
    return relative_risk;
}

double DefaultDiseaseModel::calculate_relative_risk_for_risk_factors(const Person &person) const {
    double relative_risk = 1.0;
    const auto &relative_factors = definition_.get().relative_risk_factors();
    for (const auto &factor : person.risk_factors) {
        if (!relative_factors.contains(factor.first)) {
            continue;
        }

        const auto &lut = relative_factors.at(factor.first).at(person.gender);
        auto factor_value =
            weight_classifier_.adjust_risk_factor_value(person, factor.first, factor.second);
        auto lookup_value = static_cast<float>(factor_value);
        auto relative_factor_value = lut(person.age, lookup_value);
        relative_risk *= relative_factor_value;
    }

    return relative_risk;
}

double DefaultDiseaseModel::calculate_relative_risk_for_diseases(const Person &person,
                                                                 int start_time,
                                                                 int time_now) const {
    double relative_risk = 1.0;
    const auto &lut = definition_.get().relative_risk_diseases();
    for (const auto &disease : person.diseases) {
        if (!lut.contains(disease.first)) {
            continue;
        }

        // Only include existing diseases
        if (disease.second.status == DiseaseStatus::active &&
            (start_time == time_now || disease.second.start_time < time_now)) {
            auto relative_disease_vale = lut.at(disease.first)(person.age, person.gender);
            relative_risk *= relative_disease_vale;
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

        auto relative_risk =
            calculate_combined_relative_risk(person, context.start_time(), context.time_now());
        auto average_relative_risk = average_relative_risk_.at(person.age, person.gender);
        auto incidence = definition_.get().table()(person.age, person.gender).at(incidence_id);
        auto probability = incidence * relative_risk / average_relative_risk;
        auto hazard = context.random().next_double();
        if (hazard < probability) {
            person.diseases[disease_type()] =
                Disease{.status = DiseaseStatus::active, .start_time = context.time_now()};
        }
    }
}

} // namespace hgps
