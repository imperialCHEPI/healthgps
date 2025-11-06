#include "default_disease_model.h"
#include "HealthGPS.Input/pif_data.h"
#include "person.h"
#include "runtime_context.h"

#include <atomic>
#include <chrono>
#include <fmt/color.h>
#include <iostream>
#include <mutex>
#include <oneapi/tbb/parallel_for_each.h>
#include <set>

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
            // start_time = 0 means the disease existed before the simulation started.
            person.diseases[disease_type()] =
                Disease{.status = DiseaseStatus::active, .start_time = 0};
        }
    }
}

void DefaultDiseaseModel::initialise_average_relative_risk(RuntimeContext &context) {
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void DefaultDiseaseModel::update_incidence_cases(RuntimeContext &context) {
    // PHASE 1: Move ALL lookups outside parallel loop to avoid race conditions and improve
    // performance

    // 1. Cache disease type ID (to avoid calling disease_type() in loop)
    const auto &disease_type_id = disease_type();

    // 2. Cache measures table and incidence_id (currently called inside loop on line 252)
    const auto &measures_table = definition_.get().table();
    int incidence_id = measures_table.at(MeasureKey::incidence);

    // 3. Cache PIF data if exists (currently checked inside loop on lines 256-257, 263-264)
    bool has_pif_data = definition_.get().has_pif_data();
    ScenarioType scenario_type = context.scenario().type();

    const hgps::input::PIFTable *pif_table = nullptr;
    int year_post_intervention = context.time_now() - context.start_time();

    if (has_pif_data && scenario_type == ScenarioType::intervention) {
        const auto &pif_data = definition_.get().pif_data();
        const auto &pif_config = context.inputs().population_impact_fraction();
        pif_table = pif_data.get_scenario_data(pif_config.scenario);
    }

    // PHASE 3: Thread-safe PIF print flag
    static std::once_flag pif_print_once_flag;

    // Pre-extract tables to vectors for faster access in parallel loop
    const auto &age_range = context.age_range();
    const int num_ages = age_range.length() + 1;
    const int age_min = age_range.lower();
    constexpr int gender_male = 0;
    constexpr int gender_female = 1;

    // Create age range vector for parallel extraction
    std::vector<int> ages;
    ages.reserve(num_ages);
    for (int age = age_min; age <= age_range.upper(); age++) {
        ages.push_back(age);
    }

    // Pre-extract incidence table to vector
    std::vector<std::vector<double>> incidence_table(num_ages, std::vector<double>(2));
    tbb::parallel_for_each(ages.begin(), ages.end(), [&](int age) {
        int age_idx = age - age_min;
        incidence_table[age_idx][gender_male] =
            measures_table(age, core::Gender::male).at(incidence_id);
        incidence_table[age_idx][gender_female] =
            measures_table(age, core::Gender::female).at(incidence_id);
    });

    // Pre-extract average relative risk table to vector
    std::vector<std::vector<double>> avg_rr_table(num_ages, std::vector<double>(2));
    tbb::parallel_for_each(ages.begin(), ages.end(), [&](int age) {
        int age_idx = age - age_min;
        avg_rr_table[age_idx][gender_male] = average_relative_risk_.at(age, core::Gender::male);
        avg_rr_table[age_idx][gender_female] = average_relative_risk_.at(age, core::Gender::female);
    });

    // Pre-extract PIF table to vector if PIF data is available
    std::vector<std::vector<double>> pif_table_local(num_ages, std::vector<double>(2));
    if (pif_table != nullptr) {
        tbb::parallel_for_each(ages.begin(), ages.end(), [&](int age) {
            int age_idx = age - age_min;
            pif_table_local[age_idx][gender_male] =
                pif_table->get_pif_value(age, core::Gender::male, year_post_intervention);
            pif_table_local[age_idx][gender_female] =
                pif_table->get_pif_value(age, core::Gender::female, year_post_intervention);
        });
    }

// Manual PIF Debug (only compiled if DEBUG_PIF is defined)
#ifdef DEBUG_PIF
    if (pif_table != nullptr) {
        static std::set<std::string> debug_diseases_printed;
        if (debug_diseases_printed.find(disease_type_id.to_string()) ==
            debug_diseases_printed.end()) {
            std::cout << "=== PIF DEBUG FOR DISEASE: " << disease_type_id.to_string() << " ==="
                      << '\n';
            std::cout << "PIF Table Size: " << pif_table->size() << '\n';

            int debug_age = 55;
            core::Gender debug_gender = core::Gender::female;
            int debug_year = 17;

            double debug_pif =
                pif_table->get_pif_value(debug_age, debug_gender, debug_year);

            std::cout << "PIF Debug: Disease=" << disease_type_id.to_string() << ", Age=" << debug_age
                      << ", Gender=" << (debug_gender == core::Gender::male ? "Male" : "Female")
                      << ", YearPostInt=" << debug_year << ", PIFValue=" << debug_pif << '\n';

            std::cout << "=== END PIF DEBUG FOR " << disease_type_id.to_string() << " ===" << '\n'
                      << '\n';

            debug_diseases_printed.insert(disease_type_id.to_string());
        }
    }
#endif

    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << "Start update_incidence_cases: " << disease_type() << "\n";
    fflush(stderr);
    fflush(stdout);

    // MAHIMA: Process people in parallel - each person's disease calculation is independent
    // All expensive lookups are now pre-computed above
    tbb::parallel_for_each(
        context.population().begin(), context.population().end(), [&](auto &person) {
            // Skip if person is inactive.
            if (!person.is_active()) {
                return;
            }

            // Clear newborn diseases.
            if (person.age == 0) {
                person.diseases.clear();
                return;
            }

            // Skip if the person already has the disease.
            // Use cached disease_type_id instead of calling disease_type()
            if (person.diseases.contains(disease_type_id) &&
                person.diseases.at(disease_type_id).status == DiseaseStatus::active) {
                return;
            }

            double relative_risk = 1.0;
            relative_risk *= calculate_relative_risk_for_risk_factors(person);
            relative_risk *= calculate_relative_risk_for_diseases(person);

            int age_idx = person.age - age_min;
            int gender_idx = (person.gender == core::Gender::male) ? gender_male : gender_female;

            double average_relative_risk = avg_rr_table[age_idx][gender_idx];
            double incidence = incidence_table[age_idx][gender_idx];
            double probability = incidence * relative_risk / average_relative_risk;

            // Apply PIF adjustment if PIF data is available and we're in intervention scenario
            // Use cached PIF data instead of repeated definition_.get() calls
            if (pif_table != nullptr) {
                // THREAD-SAFE: Print confirmation message once
                std::call_once(pif_print_once_flag, []() {
                    fmt::print(fg(fmt::color::green),
                               "PIF Analysis: Applying Population Impact Fraction adjustments to "
                               "disease incidence calculations\n");
                });

                double pif_value = pif_table_local[age_idx][gender_idx];
                probability *= (1.0 - pif_value);
            }

            double hazard = context.random().next_double();
            if (hazard < probability) {
                // Use cached disease_type_id instead of calling disease_type()
                person.diseases[disease_type_id] =
                    Disease{.status = DiseaseStatus::active, .start_time = context.time_now()};
            }
        });
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "End update_incidence_cases: " << disease_type() << " (took " << duration.count()
              << "ms)\n";
    fflush(stderr);
    fflush(stdout);
}

} // namespace hgps
