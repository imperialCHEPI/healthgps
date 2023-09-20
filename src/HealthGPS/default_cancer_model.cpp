#include "default_cancer_model.h"
#include "HealthGPS.Core/thread_util.h"
#include "person.h"
#include "runtime_context.h"

namespace hgps {

DefaultCancerModel::DefaultCancerModel(DiseaseDefinition &definition, WeightModel &&classifier,
                                       const core::IntegerInterval &age_range)
    : definition_{definition}, weight_classifier_{std::move(classifier)},
      average_relative_risk_{create_age_gender_table<double>(age_range)} {
    if (definition_.get().identifier().group != core::DiseaseGroup::cancer) {
        throw std::invalid_argument("Disease definition group mismatch, must be 'cancer'.");
    }
}

core::DiseaseGroup DefaultCancerModel::group() const noexcept {
    return definition_.get().identifier().group;
}

const core::Identifier &DefaultCancerModel::disease_type() const noexcept {
    return definition_.get().identifier().code;
}

void DefaultCancerModel::initialise_disease_status(RuntimeContext &context) {
    auto relative_risk_table = calculate_average_relative_risk(context);
    auto prevalence_id = definition_.get().table().at(MeasureKey::prevalence);
    for (auto &entity : context.population()) {
        if (!entity.is_active() || !definition_.get().table().contains(entity.age)) {
            continue;
        }

        auto relative_risk_value = calculate_relative_risk_for_risk_factors(entity);
        auto average_relative_risk = relative_risk_table(entity.age, entity.gender);
        auto prevalence = definition_.get().table()(entity.age, entity.gender).at(prevalence_id);
        auto probability = prevalence * relative_risk_value / average_relative_risk;
        auto hazard = context.random().next_double();
        if (hazard < probability) {
            auto onset = calculate_time_since_onset(context, entity.gender);
            entity.diseases[disease_type()] = Disease{.status = DiseaseStatus::active,
                                                      .start_time = context.time_now(),
                                                      .time_since_onset = onset};
        }
    }
}

void DefaultCancerModel::initialise_average_relative_risk(RuntimeContext &context) {
    const auto &age_range = context.age_range();
    auto sum = create_age_gender_table<double>(age_range);
    auto count = create_age_gender_table<double>(age_range);
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};
    std::for_each(core::execution_policy, pop.cbegin(), pop.cend(), [&](const auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        auto combine_risk =
            calculate_combined_relative_risk(entity, context.start_time(), context.time_now());
        auto lock = std::unique_lock{sum_mutex};
        sum(entity.age, entity.gender) += combine_risk;
        count(entity.age, entity.gender)++;
    });

    auto default_average = 1.0;
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

double DefaultCancerModel::get_excess_mortality(const Person &entity) const noexcept {
    const auto &disease_info = entity.diseases.at(disease_type());
    auto max_onset = definition_.get().parameters().max_time_since_onset;
    if (disease_info.time_since_onset < 0 || disease_info.time_since_onset >= max_onset) {
        return 0.0;
    }

    auto mortality_id = definition_.get().table().at(MeasureKey::mortality);
    auto excess_mortality = definition_.get().table()(entity.age, entity.gender).at(mortality_id);
    const auto &death_weight =
        definition_.get().parameters().death_weight.at(disease_info.time_since_onset);
    if (entity.gender == core::Gender::male) {
        return excess_mortality * death_weight.males;
    }

    return excess_mortality * death_weight.females;
}

DoubleAgeGenderTable DefaultCancerModel::calculate_average_relative_risk(RuntimeContext &context) {
    const auto &age_range = context.age_range();
    auto sum = create_age_gender_table<double>(age_range);
    auto count = create_age_gender_table<double>(age_range);
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};
    std::for_each(core::execution_policy, pop.cbegin(), pop.cend(), [&](const auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        auto combine_risk = calculate_relative_risk_for_risk_factors(entity);
        auto lock = std::unique_lock{sum_mutex};
        sum(entity.age, entity.gender) += combine_risk;
        count(entity.age, entity.gender)++;
    });

    auto default_average = 1.0;
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

double DefaultCancerModel::calculate_combined_relative_risk(const Person &entity, int start_time,
                                                            int time_now) const {
    auto combined_risk_value = 1.0;
    combined_risk_value *= calculate_relative_risk_for_risk_factors(entity);
    combined_risk_value *= calculate_relative_risk_for_diseases(entity, start_time, time_now);
    return combined_risk_value;
}

double DefaultCancerModel::calculate_relative_risk_for_risk_factors(const Person &entity) const {
    auto relative_risk_value = 1.0;
    const auto &relative_factors = definition_.get().relative_risk_factors();
    for (const auto &factor : entity.risk_factors) {
        if (!relative_factors.contains(factor.first)) {
            continue;
        }

        const auto &lut = relative_factors.at(factor.first).at(entity.gender);
        auto factor_value =
            weight_classifier_.adjust_risk_factor_value(entity, factor.first, factor.second);
        auto lookup_value = static_cast<float>(factor_value);
        auto relative_factor_value = lut(entity.age, lookup_value);
        relative_risk_value *= relative_factor_value;
    }

    return relative_risk_value;
}

double DefaultCancerModel::calculate_relative_risk_for_diseases(const Person &entity,
                                                                int start_time,
                                                                int time_now) const {
    auto relative_risk_value = 1.0;
    const auto &lut = definition_.get().relative_risk_diseases();
    for (const auto &disease : entity.diseases) {
        if (!lut.contains(disease.first)) {
            continue;
        }

        // Only include existing diseases
        if (disease.second.status == DiseaseStatus::active &&
            (start_time == time_now || disease.second.start_time < time_now)) {
            auto relative_disease_vale = lut.at(disease.first)(entity.age, entity.gender);

            relative_risk_value *= relative_disease_vale;
        }
    }

    return relative_risk_value;
}

void DefaultCancerModel::update_remission_cases(RuntimeContext &context) {
    auto max_onset = definition_.get().parameters().max_time_since_onset;
    auto &pop = context.population();
    std::for_each(core::execution_policy, pop.begin(), pop.end(), [&](auto &entity) {
        if (entity.age == 0 || !entity.is_active()) {
            return;
        }

        if (!entity.diseases.contains(disease_type()) ||
            entity.diseases.at(disease_type()).status != DiseaseStatus::active) {
            return;
        }

        // Increment duration by one year
        auto &disease = entity.diseases.at(disease_type());
        disease.time_since_onset++;
        if (disease.time_since_onset >= max_onset) {
            disease.status = DiseaseStatus::free;
            disease.time_since_onset = -1;
        }
    });
}

void DefaultCancerModel::update_incidence_cases(RuntimeContext &context) {
    for (auto &entity : context.population()) {
        if (!entity.is_active()) {
            continue;
        }

        if (entity.age == 0) {
            if (!entity.diseases.empty()) {
                entity.diseases.clear(); // Should not have nay disease at birth!
            }

            continue;
        }

        // Already have disease
        if (entity.diseases.contains(disease_type()) &&
            entity.diseases.at(disease_type()).status == DiseaseStatus::active) {
            continue;
        }

        auto probability =
            calculate_incidence_probability(entity, context.start_time(), context.time_now());
        auto hazard = context.random().next_double();
        if (hazard < probability) {
            entity.diseases[disease_type()] = Disease{.status = DiseaseStatus::active,
                                                      .start_time = context.time_now(),
                                                      .time_since_onset = 0};
        }
    }
}

double DefaultCancerModel::calculate_incidence_probability(const Person &entity, int start_time,
                                                           int time_now) const {
    auto incidence_id = definition_.get().table().at(MeasureKey::incidence);
    auto combined_relative_risk = calculate_combined_relative_risk(entity, start_time, time_now);
    auto average_relative_risk = average_relative_risk_.at(entity.age, entity.gender);
    auto incidence = definition_.get().table()(entity.age, entity.gender).at(incidence_id);
    auto probability = incidence * combined_relative_risk / average_relative_risk;
    return probability;
}

int DefaultCancerModel::calculate_time_since_onset(RuntimeContext &context,
                                                   const core::Gender &gender) const {

    const auto &pdf = definition_.get().parameters().prevalence_distribution;
    auto values = std::vector<int>{};
    auto cumulative = std::vector<double>{};
    auto sum = 0.0;
    for (const auto &item : pdf) {
        auto p = item.second.males;
        if (gender != core::Gender::male) {
            p = item.second.females;
        }

        sum += p;
        values.emplace_back(item.first);
        cumulative.emplace_back(sum);
    }

    return context.random().next_empirical_discrete(values, cumulative);
}
} // namespace hgps
