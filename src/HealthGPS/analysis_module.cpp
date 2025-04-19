#include "HealthGPS.Core/thread_util.h"

#include "analysis_module.h"
#include "converter.h"
#include "lms_model.h"
#include "weight_model.h"

#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <oneapi/tbb/parallel_for_each.h>

namespace hgps {

/// @brief DALYs result unit conversion constant.
inline constexpr double DALY_UNITS = 100'000.0;

AnalysisModule::AnalysisModule(AnalysisDefinition &&definition, WeightModel &&classifier,
                               const core::IntegerInterval age_range, unsigned int comorbidities)
    : definition_{std::move(definition)}, weight_classifier_{std::move(classifier)},
      residual_disability_weight_{create_age_gender_table<double>(age_range)},
      comorbidities_{comorbidities} {
    // std::cout << "\nDEBUG: AnalysisModule constructor called with max age: " << age_range.upper()
    // << std::endl;
}

// Overload constructor with additional parameter for calculated_stats_
AnalysisModule::AnalysisModule(AnalysisDefinition &&definition, WeightModel &&classifier,
                               const core::IntegerInterval age_range, unsigned int comorbidities,
                               std::vector<double> calculated_stats)
    : definition_{std::move(definition)}, weight_classifier_{std::move(classifier)},
      residual_disability_weight_{create_age_gender_table<double>(age_range)},
      comorbidities_{comorbidities}, calculated_stats_{std::move(calculated_stats)} {}
SimulationModuleType AnalysisModule::type() const noexcept {
    return SimulationModuleType::Analysis;
}

void AnalysisModule::initialise_vector(RuntimeContext &context) {
    try {
        std::cout << "\nDEBUG: initialise_vector - Starting" << std::endl;

        // Validate factors_to_calculate_ against available factors
        auto available_factors = std::vector<core::Identifier>();
        for (const auto &factor : context.mapping().entries()) {
            available_factors.push_back(factor.key());
        }

        // Remove any factors that aren't in the available factors list
        auto it = std::remove_if(factors_to_calculate_.begin(), factors_to_calculate_.end(),
                                 [&available_factors](const core::Identifier &factor) {
                                     bool exists = std::find(available_factors.begin(),
                                                             available_factors.end(),
                                                             factor) != available_factors.end();
                                     if (!exists) {
                                         std::cout << "\nDEBUG: Removing unavailable factor: "
                                                   << factor.to_string() << std::endl;
                                     }
                                     return !exists;
                                 });

        factors_to_calculate_.erase(it, factors_to_calculate_.end());

        factor_bins_.reserve(factors_to_calculate_.size());
        factor_bin_widths_.reserve(factors_to_calculate_.size());
        factor_min_values_.reserve(factors_to_calculate_.size());

        for (const auto &factor : factors_to_calculate_) {
            try {
                const auto [min, max] = std::ranges::minmax_element(
                    context.population(), [&factor](const auto &entity1, const auto &entity2) {
                        return entity1.get_risk_factor_value(factor) <
                               entity2.get_risk_factor_value(factor);
                    });

                double min_factor = min->get_risk_factor_value(factor);
                double max_factor = max->get_risk_factor_value(factor);

                factor_min_values_.push_back(min_factor);

                // The number of bins to use for each factor is the number of integer values of the
                // factor, or 100 bins of equal size, whichever is smaller (100 is an arbitrary
                // number, it could be any other number depending on the desired resolution of the
                // map)
                factor_bins_.push_back(std::min(100, static_cast<int>(max_factor - min_factor)));

                // The width of each bin is the range of the factor divided by the number of bins
                factor_bin_widths_.push_back((max_factor - min_factor) / factor_bins_.back());
            } catch (const std::exception &e) {
                std::cout << "\nDEBUG: Error initializing factor " << factor.to_string() << ": "
                          << e.what() << std::endl;
                // Skip this factor
            }
        }

        // The number of factors to calculate stats for is the number of factors minus the length of
        // the `factors` vector.
        size_t num_stats_to_calc =
            context.mapping().entries().size() - factors_to_calculate_.size();

        // And for each factor, we calculate the stats described in `channels_`, so we
        // multiply the size of `channels_` by the number of factors to calculate stats for.
        num_stats_to_calc *= channels_.size();

        // The product of the number of bins for each factor can be used to calculate the size of
        // the `calculated_stats_` in the next step
        size_t total_num_bins = std::accumulate(factor_bins_.cbegin(), factor_bins_.cend(),
                                                size_t{1}, std::multiplies<>());

        // Set the vector size and initialise all values to 0.0
        calculated_stats_.resize(total_num_bins * num_stats_to_calc);
        std::ranges::fill(calculated_stats_, 0.0);

        std::cout << "\nDEBUG: initialise_vector - Completed" << std::endl;
    } catch (const std::exception &e) {
        std::cout << "\nDEBUG: Exception in initialise_vector: " << e.what() << std::endl;
        // Initialize with minimal data to allow continued execution
        calculated_stats_.resize(1);
        calculated_stats_[0] = 0.0;
    }
}

const std::string &AnalysisModule::name() const noexcept { return name_; }

void AnalysisModule::initialise_population(RuntimeContext &context) {
    std::cout << "\nDEBUG: AnalysisModule::initialise_population - Starting" << std::endl;
    std::cout << "\nDEBUG: Age range: [" << context.age_range().lower() << ", "
              << context.age_range().upper() << "]" << std::endl;

    const auto &age_range = context.age_range();
    auto expected_sum = create_age_gender_table<double>(age_range);
    auto expected_count = create_age_gender_table<int>(age_range);
    auto &pop = context.population();

    std::cout << "\nDEBUG: Population size: " << pop.size() << std::endl;

    auto sum_mutex = std::mutex{};
    try {
        std::cout << "\nDEBUG: Starting parallel processing of population" << std::endl;
        tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
            if (!entity.is_active()) {
                return;
            }

            // Skip if age is out of bounds - fix signed/unsigned comparison
            if (entity.age < static_cast<int>(age_range.lower()) ||
                entity.age > static_cast<int>(age_range.upper())) {
                return;
            }

            auto sum = 1.0;
            for (const auto &disease : entity.diseases) {
                if (disease.second.status == DiseaseStatus::active &&
                    definition_.disability_weights().contains(disease.first)) {
                    sum *= (1.0 - definition_.disability_weights().at(disease.first));
                }
            }

            auto lock = std::unique_lock{sum_mutex};
            expected_sum(entity.age, entity.gender) += sum;
            expected_count(entity.age, entity.gender)++;
        });
        std::cout << "\nDEBUG: Finished parallel processing of population" << std::endl;
    } catch (const std::exception &e) {
        std::cout << "\nDEBUG: Exception in parallel_for_each: " << e.what() << std::endl;
    }

    std::cout << "\nDEBUG: Calculating residual disability weights" << std::endl;
    for (int age = age_range.lower(); age <= age_range.upper(); age++) {
        residual_disability_weight_(age, core::Gender::male) = calculate_residual_disability_weight(
            age, core::Gender::male, expected_sum, expected_count);

        residual_disability_weight_(age, core::Gender::female) =
            calculate_residual_disability_weight(age, core::Gender::female, expected_sum,
                                                 expected_count);
    }

    std::cout << "\nDEBUG: Initializing output channels" << std::endl;
    initialise_output_channels(context);

    std::cout << "\nDEBUG: Publishing result message" << std::endl;
    publish_result_message(context);

    std::cout << "\nDEBUG: AnalysisModule::initialise_population - Completed" << std::endl;
}

void AnalysisModule::update_population(RuntimeContext &context) {

    // Reset the calculated factors vector to 0.0
    std::ranges::fill(calculated_stats_, 0.0);

    publish_result_message(context);
}

double
AnalysisModule::calculate_residual_disability_weight(int age, const core::Gender gender,
                                                     const DoubleAgeGenderTable &expected_sum,
                                                     const IntegerAgeGenderTable &expected_count) {
    auto residual_value = 0.0;
    if (!expected_sum.contains(age) || !definition_.observed_YLD().contains(age)) {
        return residual_value;
    }

    auto denominator = expected_count(age, gender);
    if (denominator != 0.0) {
        auto expected_mean = expected_sum(age, gender) / denominator;
        auto observed_YLD = definition_.observed_YLD()(age, gender);
        residual_value = 1.0 - (1.0 - observed_YLD) / expected_mean;
        if (std::isnan(residual_value)) {
            residual_value = 0.0;
        }
    }

    return residual_value;
}

void AnalysisModule::publish_result_message(RuntimeContext &context) const {
    std::cout << "\nDEBUG: AnalysisModule::publish_result_message - Starting" << std::endl;

    try {
        auto sample_size = context.age_range().upper() + 1u;
        std::cout << "\nDEBUG: Sample size: " << sample_size << std::endl;

        auto result = ModelResult{sample_size};

        std::cout << "\nDEBUG: Starting calculate_historical_statistics via async" << std::endl;

        std::shared_future<void> handle;
        try {
            handle = core::run_async(&AnalysisModule::calculate_historical_statistics, this,
                                     std::ref(context), std::ref(result));
        } catch (const std::exception &e) {
            std::cout << "\nDEBUG: Exception launching calculate_historical_statistics: "
                      << e.what() << std::endl;
            throw;
        }

        std::cout << "\nDEBUG: Starting calculate_population_statistics" << std::endl;
        try {
            calculate_population_statistics(context, result.series);
        } catch (const std::exception &e) {
            std::cout << "\nDEBUG: Exception in calculate_population_statistics: " << e.what()
                      << std::endl;
            // Continue with the rest of the code
        }

        std::cout << "\nDEBUG: Waiting for calculate_historical_statistics to complete"
                  << std::endl;
        try {
            handle.get();
        } catch (const std::exception &e) {
            std::cout << "\nDEBUG: Exception while waiting for calculate_historical_statistics: "
                      << e.what() << std::endl;
            // Continue with result publishing
        }

        std::cout << "\nDEBUG: Publishing result event message" << std::endl;
        try {
            context.publish(std::make_unique<ResultEventMessage>(
                context.identifier(), context.current_run(), context.time_now(), result));
        } catch (const std::exception &e) {
            std::cout << "\nDEBUG: Exception publishing result message: " << e.what() << std::endl;
            // Nothing to do if publishing fails
        }

        std::cout << "\nDEBUG: AnalysisModule::publish_result_message - Completed" << std::endl;
    } catch (const std::exception &e) {
        std::cout << "\nDEBUG: CRITICAL EXCEPTION in publish_result_message: " << e.what()
                  << std::endl;
        // Create a minimal result to publish in case of critical error
        try {
            auto sample_size = context.age_range().upper() + 1u;
            auto result = ModelResult{sample_size};
            context.publish(std::make_unique<ResultEventMessage>(
                context.identifier(), context.current_run(), context.time_now(), result));
            std::cout << "\nDEBUG: Published minimal result after critical exception" << std::endl;
        } catch (...) {
            std::cout << "\nDEBUG: Failed to publish minimal result" << std::endl;
        }
    }
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
void AnalysisModule::calculate_historical_statistics(RuntimeContext &context,
                                                     ModelResult &result) const {
    std::cout << "\nDEBUG: AnalysisModule::calculate_historical_statistics - Starting" << std::endl;

    try {
        auto risk_factors = std::map<core::Identifier, std::map<core::Gender, double>>();
        for (const auto &item : context.mapping()) {
            if (item.level() > 0) {
                risk_factors.emplace(item.key(), std::map<core::Gender, double>{});
            }
        }

        auto prevalence = std::map<core::Identifier, std::map<core::Gender, int>>();
        for (const auto &item : context.diseases()) {
            prevalence.emplace(item.code, std::map<core::Gender, int>{});
        }

        auto comorbidity = std::map<unsigned int, ResultByGender>{};
        for (auto i = 0u; i <= comorbidities_; i++) {
            comorbidity.emplace(i, ResultByGender{});
        }

        auto gender_age_sum = std::map<core::Gender, int>{};
        auto gender_count = std::map<core::Gender, int>{};
        auto age_upper_bound = context.age_range().upper();
        auto analysis_time = static_cast<unsigned int>(context.time_now());

        std::cout << "\nDEBUG: Starting calculate_dalys via async" << std::endl;
        auto daly_handle =
            core::run_async(&AnalysisModule::calculate_dalys, this, std::ref(context.population()),
                            age_upper_bound, analysis_time);

        auto population_size = static_cast<int>(context.population().size());
        auto population_dead = 0;
        auto population_migrated = 0;

        std::cout << "\nDEBUG: Processing population for statistics" << std::endl;

        try {
            // Process each entity in the population
            for (const auto &entity : context.population()) {
                // Skip processing if age is invalid - fix signed/unsigned comparison
                if (entity.age < 0 || entity.age > static_cast<int>(age_upper_bound)) {
                    continue;
                }

                try {
                    if (!entity.is_active()) {
                        if (entity.has_emigrated() && entity.time_of_migration() == analysis_time) {
                            population_migrated++;
                        }

                        if (!entity.is_alive() && entity.time_of_death() == analysis_time) {
                            population_dead++;
                        }

                        continue;
                    }

                    gender_age_sum[entity.gender] += entity.age;
                    gender_count[entity.gender]++;

                    for (auto &item : risk_factors) {
                        auto factor_value = entity.get_risk_factor_value(item.first);
                        if (std::isnan(factor_value)) {
                            factor_value = 0.0;
                        }

                        item.second[entity.gender] += factor_value;
                    }

                    auto comorbidity_number = 0u;
                    for (const auto &item : entity.diseases) {
                        if (item.second.status == DiseaseStatus::active) {
                            comorbidity_number++;
                            prevalence.at(item.first)[entity.gender]++;
                        }
                    }

                    if (comorbidity_number > comorbidities_) {
                        comorbidity_number = comorbidities_;
                    }

                    if (entity.gender == core::Gender::male) {
                        comorbidity[comorbidity_number].male++;
                    } else {
                        comorbidity[comorbidity_number].female++;
                    }
                } catch (const std::exception &e) {
                    std::cout << "\nDEBUG: Exception processing entity: " << e.what() << std::endl;
                    // Continue processing other entities
                }
            }

            std::cout << "\nDEBUG: Finished processing population statistics" << std::endl;

            // Calculate the averages avoiding division by zero
            auto males_count = std::max(1, gender_count[core::Gender::male]);
            auto females_count = std::max(1, gender_count[core::Gender::female]);
            result.population_size = population_size;
            result.number_alive = IntegerGenderValue{males_count, females_count};
            result.number_dead = population_dead;
            result.number_emigrated = population_migrated;
            result.average_age.male = gender_age_sum[core::Gender::male] * 1.0 / males_count;
            result.average_age.female = gender_age_sum[core::Gender::female] * 1.0 / females_count;

            std::cout << "\nDEBUG: Processing risk factors" << std::endl;
            for (auto &item : risk_factors) {
                auto user_name = context.mapping().at(item.first).name();
                result.risk_ractor_average.emplace(
                    user_name,
                    ResultByGender{.male = item.second[core::Gender::male] / males_count,
                                   .female = item.second[core::Gender::female] / females_count});
            }

            std::cout << "\nDEBUG: Processing disease prevalence" << std::endl;
            for (const auto &item : context.diseases()) {
                result.disease_prevalence.emplace(
                    item.code.to_string(),
                    ResultByGender{.male = prevalence.at(item.code)[core::Gender::male] * 100.0 /
                                           males_count,
                                   .female = prevalence.at(item.code)[core::Gender::female] *
                                             100.0 / females_count});
            }

            std::cout << "\nDEBUG: Processing metrics" << std::endl;
            for (const auto &item : context.metrics()) {
                result.metrics.emplace(item.first, item.second);
            }

            std::cout << "\nDEBUG: Processing comorbidity" << std::endl;
            for (const auto &item : comorbidity) {
                result.comorbidity.emplace(
                    item.first,
                    ResultByGender{.male = item.second.male * 100.0 / males_count,
                                   .female = item.second.female * 100.0 / females_count});
            }

            std::cout << "\nDEBUG: Waiting for calculate_dalys to complete" << std::endl;
            try {
                result.indicators = daly_handle.get();
            } catch (const std::exception &e) {
                std::cout << "\nDEBUG: Exception in daly_handle.get(): " << e.what() << std::endl;
                // Set default values if calculate_dalys fails
                result.indicators = DALYsIndicator{.years_of_life_lost = 0.0,
                                                   .years_lived_with_disability = 0.0,
                                                   .disability_adjusted_life_years = 0.0};
            }

            std::cout << "\nDEBUG: AnalysisModule::calculate_historical_statistics - Completed"
                      << std::endl;
        } catch (const std::exception &e) {
            std::cout << "\nDEBUG: Exception in population processing loop: " << e.what()
                      << std::endl;
            throw; // Re-throw to outer handler
        }
    } catch (const std::exception &e) {
        std::cout << "\nDEBUG: CRITICAL EXCEPTION in calculate_historical_statistics: " << e.what()
                  << std::endl;
        // Set default values for result to prevent further errors
        result.population_size = static_cast<int>(context.population().size());
        result.number_alive = IntegerGenderValue{0, 0};
        result.number_dead = 0;
        result.number_emigrated = 0;
        result.average_age = ResultByGender{0.0, 0.0};
        result.indicators = DALYsIndicator{0.0, 0.0, 0.0};
    }
}
// NOLINTEND(readability-function-cognitive-complexity)

double AnalysisModule::calculate_disability_weight(const Person &entity) const {
    try {
        // Check if age is within range before proceeding
        if (!residual_disability_weight_.contains(entity.age, entity.gender)) {
            std::cout << "\nDEBUG: Age " << entity.age << " and gender "
                      << static_cast<int>(entity.gender)
                      << " not found in residual_disability_weight_ table" << std::endl;
            return 0.0;
        }

        auto sum = 1.0;
        for (const auto &disease : entity.diseases) {
            try {
                if (disease.second.status == DiseaseStatus::active) {
                    if (definition_.disability_weights().contains(disease.first)) {
                        sum *= (1.0 - definition_.disability_weights().at(disease.first));
                    }
                }
            } catch (const std::exception &e) {
                std::cout
                    << "\nDEBUG: Exception processing disease in calculate_disability_weight: "
                    << e.what() << std::endl;
                // Continue with next disease
            }
        }

        auto residual_dw = residual_disability_weight_.at(entity.age, entity.gender);
        residual_dw = std::min(1.0, std::max(residual_dw, 0.0));
        sum *= (1.0 - residual_dw);

        double result = 1.0 - sum;
        // Ensure result is valid
        if (std::isnan(result) || std::isinf(result)) {
            std::cout << "\nDEBUG: Invalid disability weight calculated (NaN or Inf), returning 0.0"
                      << std::endl;
            return 0.0;
        }

        return result;
    } catch (const std::exception &e) {
        std::cout << "\nDEBUG: Exception in calculate_disability_weight for entity age "
                  << entity.age << ", gender " << static_cast<int>(entity.gender) << ": "
                  << e.what() << std::endl;
        return 0.0;
    }
}

DALYsIndicator AnalysisModule::calculate_dalys(Population &population, unsigned int max_age,
                                               unsigned int death_year) const {
    try {
        std::cout << "\nDEBUG: calculate_dalys - Starting" << std::endl;
        auto yll_sum = 0.0;
        auto yld_sum = 0.0;
        auto count = 0.0;

        for (const auto &entity : population) {
            try {
                if (entity.time_of_death() == death_year && entity.age <= max_age) {
                    try {
                        auto male_reference_age =
                            definition_.life_expectancy().at(death_year, core::Gender::male);
                        auto female_reference_age =
                            definition_.life_expectancy().at(death_year, core::Gender::female);

                        auto reference_age = std::max(male_reference_age, female_reference_age);
                        auto lifeExpectancy = std::max(reference_age - entity.age, 0.0f);
                        yll_sum += lifeExpectancy;
                    } catch (const std::exception &e) {
                        std::cout << "\nDEBUG: Exception calculating life expectancy: " << e.what()
                                  << std::endl;
                        // Continue with next entity
                    }
                }

                if (entity.is_active()) {
                    try {
                        yld_sum += calculate_disability_weight(entity);
                        count++;
                    } catch (const std::exception &e) {
                        std::cout << "\nDEBUG: Exception calculating disability weight: "
                                  << e.what() << std::endl;
                        // Continue with next entity
                    }
                }
            } catch (const std::exception &e) {
                std::cout << "\nDEBUG: Exception processing entity in calculate_dalys: " << e.what()
                          << std::endl;
                // Continue with next entity
            }
        }

        // Avoid division by zero
        if (count <= 0) {
            std::cout << "\nDEBUG: calculate_dalys - No active entities found, returning zeros"
                      << std::endl;
            return DALYsIndicator{.years_of_life_lost = 0.0,
                                  .years_lived_with_disability = 0.0,
                                  .disability_adjusted_life_years = 0.0};
        }

        auto yll = yll_sum * DALY_UNITS / count;
        auto yld = yld_sum * DALY_UNITS / count;

        std::cout << "\nDEBUG: calculate_dalys - Completed successfully" << std::endl;
        return DALYsIndicator{.years_of_life_lost = yll,
                              .years_lived_with_disability = yld,
                              .disability_adjusted_life_years = yll + yld};
    } catch (const std::exception &e) {
        std::cout << "\nDEBUG: CRITICAL EXCEPTION in calculate_dalys: " << e.what() << std::endl;
        return DALYsIndicator{.years_of_life_lost = 0.0,
                              .years_lived_with_disability = 0.0,
                              .disability_adjusted_life_years = 0.0};
    }
}

void AnalysisModule::calculate_population_statistics(RuntimeContext &context) {
    try {
        size_t num_factors_to_calculate =
            context.mapping().entries().size() - factors_to_calculate_.size();

        for (const auto &person : context.population()) {
            // Get the bin index for each factor
            std::vector<size_t> bin_indices;
            bool invalid_factor_found = false;

            for (size_t i = 0; i < factors_to_calculate_.size(); i++) {
                try {
                    double factor_value = person.get_risk_factor_value(factors_to_calculate_[i]);
                    auto bin_index = static_cast<size_t>((factor_value - factor_min_values_[i]) /
                                                         factor_bin_widths_[i]);
                    bin_indices.push_back(bin_index);
                } catch (const std::exception &e) {
                    std::cout << "\nDEBUG: Error accessing factor "
                              << factors_to_calculate_[i].to_string() << ": " << e.what()
                              << std::endl;
                    invalid_factor_found = true;
                    break;
                }
            }

            if (invalid_factor_found) {
                continue; // Skip this person if any factor could not be accessed
            }

            // Calculate the index in the calculated_stats_ vector
            size_t index = 0;
            for (size_t i = 0; i < bin_indices.size() - 1; i++) {
                size_t accumulated_bins =
                    std::accumulate(std::next(factor_bins_.cbegin(), i + 1), factor_bins_.cend(),
                                    size_t{1}, std::multiplies<>());
                index += bin_indices[i] * accumulated_bins * num_factors_to_calculate;
            }
            index += bin_indices.back() * num_factors_to_calculate;

            // Now we can add the values of the factors that are not in factors_to_calculate_
            for (const auto &factor : context.mapping().entries()) {
                if (std::find(factors_to_calculate_.cbegin(), factors_to_calculate_.cend(),
                              factor.key()) == factors_to_calculate_.cend()) {
                    try {
                        calculated_stats_[index++] += person.get_risk_factor_value(factor.key());
                    } catch (const std::exception &e) {
                        std::cout << "\nDEBUG: Error accessing factor " << factor.key().to_string()
                                  << ": " << e.what() << std::endl;
                        // Continue with next factor
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        std::cout << "\nDEBUG: Exception in calculate_population_statistics: " << e.what()
                  << std::endl;
    }
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
void AnalysisModule::calculate_population_statistics(RuntimeContext &context,
                                                     DataSeries &series) const {
    try {
        if (series.size() > 0) {
            throw std::logic_error("This should be a new object!");
        }

        series.add_channels(channels_);

        auto current_time = static_cast<unsigned int>(context.time_now());
        for (const auto &person : context.population()) {
            auto age = person.age;
            auto gender = person.gender;

            // Skip processing if age is out of bounds
            if (age >= series.size()) {
                continue;
            }

            if (!person.is_active()) {
                if (!person.is_alive() && person.time_of_death() == current_time) {
                    series(gender, "deaths").at(age)++;
                    float expcted_life =
                        definition_.life_expectancy().at(context.time_now(), gender);
                    double yll = std::max(expcted_life - age, 0.0f) * DALY_UNITS;
                    series(gender, "mean_yll").at(age) += yll;
                    series(gender, "mean_daly").at(age) += yll;
                }

                if (person.has_emigrated() && person.time_of_migration() == current_time) {
                    series(gender, "emigrations").at(age)++;
                }

                continue;
            }

            series(gender, "count").at(age)++;

            for (const auto &factor : context.mapping().entries()) {
                try {
                    series(gender, "mean_" + factor.key().to_string()).at(age) +=
                        person.get_risk_factor_value(factor.key());
                } catch (const std::exception &e) {
                    std::cout << "\nDEBUG: Error accessing factor " << factor.key().to_string()
                              << ": " << e.what() << std::endl;
                    // Continue with the next factor
                }
            }

            for (const auto &[disease_name, disease_state] : person.diseases) {
                if (disease_state.status == DiseaseStatus::active) {
                    series(gender, "prevalence_" + disease_name.to_string()).at(age)++;
                    if (disease_state.start_time == context.time_now()) {
                        series(gender, "incidence_" + disease_name.to_string()).at(age)++;
                    }
                }
            }

            double dw = calculate_disability_weight(person);
            double yld = dw * DALY_UNITS;
            series(gender, "mean_yld").at(age) += yld;
            series(gender, "mean_daly").at(age) += yld;

            classify_weight(series, person);
        }

        // For each age group in the analysis...
        const auto age_range = context.age_range();
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            double count_F = series(core::Gender::female, "count").at(age);
            double count_M = series(core::Gender::male, "count").at(age);
            double deaths_F = series(core::Gender::female, "deaths").at(age);
            double deaths_M = series(core::Gender::male, "deaths").at(age);

            // Calculate in-place factor averages.
            for (const auto &factor : context.mapping().entries()) {
                std::string column = "mean_" + factor.key().to_string();
                // Avoid division by zero
                if (count_F > 0) {
                    series(core::Gender::female, column).at(age) /= count_F;
                }
                if (count_M > 0) {
                    series(core::Gender::male, column).at(age) /= count_M;
                }
            }

            // Calculate in-place disease prevalence and incidence rates.
            for (const auto &disease : context.diseases()) {
                std::string column_prevalence = "prevalence_" + disease.code.to_string();
                // Avoid division by zero
                if (count_F > 0) {
                    series(core::Gender::female, column_prevalence).at(age) /= count_F;
                }
                if (count_M > 0) {
                    series(core::Gender::male, column_prevalence).at(age) /= count_M;
                }

                std::string column_incidence = "incidence_" + disease.code.to_string();
                // Avoid division by zero
                if (count_F > 0) {
                    series(core::Gender::female, column_incidence).at(age) /= count_F;
                }
                if (count_M > 0) {
                    series(core::Gender::male, column_incidence).at(age) /= count_M;
                }
            }

            // Calculate in-place YLL/YLD/DALY averages.
            for (const auto &column : {"mean_yll", "mean_yld", "mean_daly"}) {
                // Avoid division by zero
                if ((count_F + deaths_F) > 0) {
                    series(core::Gender::female, column).at(age) /= (count_F + deaths_F);
                }
                if ((count_M + deaths_M) > 0) {
                    series(core::Gender::male, column).at(age) /= (count_M + deaths_M);
                }
            }
        }

        // Calculate standard deviation
        calculate_standard_deviation(context, series);
    } catch (const std::exception &e) {
        std::cout << "\nDEBUG: Exception in calculate_population_statistics: " << e.what()
                  << std::endl;
        // Continue execution with partial results
    }
}
// NOLINTEND(readability-function-cognitive-complexity)

void AnalysisModule::calculate_standard_deviation(RuntimeContext &context,
                                                  DataSeries &series) const {

    // Accumulate squared deviations from mean.
    auto accumulate_squared_diffs = [&series](const std::string &chan, core::Gender sex, int age,
                                              double value) {
        // Skip if age is out of bounds
        if (age >= series.size()) {
            return;
        }
        const double mean = series(sex, "mean_" + chan).at(age);
        const double diff = value - mean;
        series(sex, "std_" + chan).at(age) += diff * diff;
    };

    auto current_time = static_cast<unsigned int>(context.time_now());
    for (const auto &person : context.population()) {
        unsigned int age = person.age;
        core::Gender sex = person.gender;

        // Skip if age is out of bounds
        if (age >= series.size()) {
            continue;
        }

        if (!person.is_active()) {
            if (!person.is_alive() && person.time_of_death() == current_time) {
                float expcted_life = definition_.life_expectancy().at(context.time_now(), sex);
                double yll = std::max(expcted_life - age, 0.0f) * DALY_UNITS;
                accumulate_squared_diffs("yll", sex, age, yll);
                accumulate_squared_diffs("daly", sex, age, yll);
            }

            continue;
        }

        double dw = calculate_disability_weight(person);
        double yld = dw * DALY_UNITS;
        accumulate_squared_diffs("yld", sex, age, yld);
        accumulate_squared_diffs("daly", sex, age, yld);

        for (const auto &factor : context.mapping().entries()) {
            const double value = person.get_risk_factor_value(factor.key());
            accumulate_squared_diffs(factor.key().to_string(), sex, age, value);
        }
    }

    // Calculate in-place standard deviation.
    auto divide_by_count_sqrt = [&series](const std::string &chan, core::Gender sex, int age,
                                          double count) {
        // Avoid division by zero
        if (count <= 0) {
            series(sex, "std_" + chan).at(age) = 0.0;
            return;
        }
        const double sum = series(sex, "std_" + chan).at(age);
        const double std = std::sqrt(sum / count);
        series(sex, "std_" + chan).at(age) = std;
    };

    // For each age group in the analysis...
    const auto age_range = context.age_range();
    for (int age = age_range.lower(); age <= age_range.upper(); age++) {
        double count_F = series(core::Gender::female, "count").at(age);
        double count_M = series(core::Gender::male, "count").at(age);
        double deaths_F = series(core::Gender::female, "deaths").at(age);
        double deaths_M = series(core::Gender::male, "deaths").at(age);

        // Calculate in-place factor standard deviation.
        for (const auto &factor : context.mapping().entries()) {
            divide_by_count_sqrt(factor.key().to_string(), core::Gender::female, age, count_F);
            divide_by_count_sqrt(factor.key().to_string(), core::Gender::male, age, count_M);
        }

        // Calculate in-place YLL/YLD/DALY standard deviation.
        for (const auto &column : {"yll", "yld", "daly"}) {
            divide_by_count_sqrt(column, core::Gender::female, age, (count_F + deaths_F));
            divide_by_count_sqrt(column, core::Gender::male, age, (count_M + deaths_M));
        }
    }
}

void AnalysisModule::classify_weight(DataSeries &series, const Person &entity) const {
    // Check if the age is within the valid range before processing
    if (entity.age >= series.size()) {
        return; // Skip this entity if age is out of bounds
    }

    auto weight_class = weight_classifier_.classify_weight(entity);
    switch (weight_class) {
    case WeightCategory::normal:
        series(entity.gender, "normal_weight").at(entity.age)++;
        break;
    case WeightCategory::overweight:
        series(entity.gender, "over_weight").at(entity.age)++;
        series(entity.gender, "above_weight").at(entity.age)++;
        break;
    case WeightCategory::obese:
        series(entity.gender, "obese_weight").at(entity.age)++;
        series(entity.gender, "above_weight").at(entity.age)++;
        break;
    default:
        throw std::logic_error("Unknown weight classification category.");
        break;
    }
}

void AnalysisModule::initialise_output_channels(RuntimeContext &context) {
    if (!channels_.empty()) {
        return;
    }

    channels_.emplace_back("count");
    channels_.emplace_back("deaths");
    channels_.emplace_back("emigrations");

    for (const auto &factor : context.mapping().entries()) {
        channels_.emplace_back("mean_" + factor.key().to_string());
        channels_.emplace_back("std_" + factor.key().to_string());
    }

    for (const auto &disease : context.diseases()) {
        channels_.emplace_back("prevalence_" + disease.code.to_string());
        channels_.emplace_back("incidence_" + disease.code.to_string());
    }

    channels_.emplace_back("normal_weight");
    channels_.emplace_back("over_weight");
    channels_.emplace_back("obese_weight");
    channels_.emplace_back("above_weight");
    channels_.emplace_back("mean_yll");
    channels_.emplace_back("std_yll");
    channels_.emplace_back("mean_yld");
    channels_.emplace_back("std_yld");
    channels_.emplace_back("mean_daly");
    channels_.emplace_back("std_daly");
}

std::unique_ptr<AnalysisModule> build_analysis_module(Repository &repository,
                                                      const ModelInput &config) {
    auto analysis_entity = repository.manager().get_disease_analysis(config.settings().country());
    auto &lms_definition = repository.get_lms_definition();
    if (lms_definition.empty()) {
        throw std::logic_error("Failed to create analysis module: invalid LMS model definition.");
    }

    auto definition = detail::StoreConverter::to_analysis_definition(analysis_entity);
    auto classifier = WeightModel{LmsModel{lms_definition}};

    return std::make_unique<AnalysisModule>(std::move(definition), std::move(classifier),
                                            config.settings().age_range(),
                                            config.run().comorbidities);
}

} // namespace hgps
