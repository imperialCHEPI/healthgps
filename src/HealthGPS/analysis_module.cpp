
#include "HealthGPS.Core/thread_util.h"

#include "analysis_module.h"
#include "converter.h"
#include "lms_model.h"
#include "weight_model.h"

#include <cmath>
#include <functional>
#include <future>
#include <oneapi/tbb/parallel_for_each.h>

namespace hgps {

/// @brief DALYs result unit conversion constant.
inline constexpr double DALY_UNITS = 100'000.0;

AnalysisModule::AnalysisModule(AnalysisDefinition &&definition, WeightModel &&classifier,
                               const core::IntegerInterval age_range, unsigned int comorbidities)
    : definition_{std::move(definition)}, weight_classifier_{std::move(classifier)},
      residual_disability_weight_{create_age_gender_table<double>(age_range)},
      comorbidities_{comorbidities} {}

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
    factor_bins_.reserve(factors_to_calculate_.size());
    factor_bin_widths_.reserve(factors_to_calculate_.size());
    factor_min_values_.reserve(factors_to_calculate_.size());

    for (const auto &factor : factors_to_calculate_) {
        const auto [min, max] = std::ranges::minmax_element(
            context.population(), [&factor](const auto &entity1, const auto &entity2) {
                return entity1.get_risk_factor_value(factor) <
                       entity2.get_risk_factor_value(factor);
            });

        double min_factor = min->get_risk_factor_value(factor);
        double max_factor = max->get_risk_factor_value(factor);

        factor_min_values_.push_back(min_factor);

        // The number of bins to use for each factor is the number of integer values of the factor,
        // or 100 bins of equal size, whichever is smaller (100 is an arbitrary number, it could be
        // any other number depending on the desired resolution of the map)
        factor_bins_.push_back(std::min(100, static_cast<int>(max_factor - min_factor)));

        // The width of each bin is the range of the factor divided by the number of bins
        factor_bin_widths_.push_back((max_factor - min_factor) / factor_bins_.back());
    }

    // The number of factors to calculate stats for is the number of factors minus the length of the
    // `factors` vector.
    size_t num_stats_to_calc = context.mapping().entries().size() - factors_to_calculate_.size();

    // And for each factor, we calculate the stats described in `channels_`, so we
    // multiply the size of `channels_` by the number of factors to calculate stats for.
    num_stats_to_calc *= channels_.size();

    // The product of the number of bins for each factor can be used to calculate the size of the
    // `calculated_stats_` in the next step
    size_t total_num_bins =
        std::accumulate(factor_bins_.cbegin(), factor_bins_.cend(), size_t{1}, std::multiplies<>());

    // Set the vector size and initialise all values to 0.0
    calculated_stats_.resize(total_num_bins * num_stats_to_calc);
}

const std::string &AnalysisModule::name() const noexcept { return name_; }

void AnalysisModule::initialise_population(RuntimeContext &context) {
    const auto &age_range = context.age_range();
    auto expected_sum = create_age_gender_table<double>(age_range);
    auto expected_count = create_age_gender_table<int>(age_range);
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};
    tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
        if (!entity.is_active()) {
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

    for (int age = age_range.lower(); age <= age_range.upper(); age++) {
        residual_disability_weight_(age, core::Gender::male) = calculate_residual_disability_weight(
            age, core::Gender::male, expected_sum, expected_count);

        residual_disability_weight_(age, core::Gender::female) =
            calculate_residual_disability_weight(age, core::Gender::female, expected_sum,
                                                 expected_count);
    }

    initialise_output_channels(context);

    publish_result_message(context);
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
    auto sample_size = context.age_range().upper() + 1u;
    auto result = ModelResult{sample_size};
    auto handle = core::run_async(&AnalysisModule::calculate_historical_statistics, this,
                                  std::ref(context), std::ref(result));

    calculate_population_statistics(context, result.series);
    handle.get();

    context.publish(std::make_unique<ResultEventMessage>(
        context.identifier(), context.current_run(), context.time_now(), result));
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
void AnalysisModule::calculate_historical_statistics(RuntimeContext &context,
                                                     ModelResult &result) const {
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

    auto daly_handle =
        core::run_async(&AnalysisModule::calculate_dalys, this, std::ref(context.population()),
                        age_upper_bound, analysis_time);

    auto population_size = static_cast<int>(context.population().size());
    auto population_dead = 0;
    auto population_migrated = 0;
    for (const auto &entity : context.population()) {
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
    }

    // Calculate the averages avoiding division by zero
    auto males_count = std::max(1, gender_count[core::Gender::male]);
    auto females_count = std::max(1, gender_count[core::Gender::female]);
    result.population_size = population_size;
    result.number_alive = IntegerGenderValue{males_count, females_count};
    result.number_dead = population_dead;
    result.number_emigrated = population_migrated;
    result.average_age.male = gender_age_sum[core::Gender::male] * 1.0 / males_count;
    result.average_age.female = gender_age_sum[core::Gender::female] * 1.0 / females_count;
    for (auto &item : risk_factors) {
        auto user_name = context.mapping().at(item.first).name();
        result.risk_ractor_average.emplace(
            user_name, ResultByGender{.male = item.second[core::Gender::male] / males_count,
                                      .female = item.second[core::Gender::female] / females_count});
    }

    for (const auto &item : context.diseases()) {
        result.disease_prevalence.emplace(
            item.code.to_string(),
            ResultByGender{
                .male = prevalence.at(item.code)[core::Gender::male] * 100.0 / males_count,
                .female = prevalence.at(item.code)[core::Gender::female] * 100.0 / females_count});
    }

    for (const auto &item : context.metrics()) {
        result.metrics.emplace(item.first, item.second);
    }

    for (const auto &item : comorbidity) {
        result.comorbidity.emplace(
            item.first, ResultByGender{.male = item.second.male * 100.0 / males_count,
                                       .female = item.second.female * 100.0 / females_count});
    }

    result.indicators = daly_handle.get();
}
// NOLINTEND(readability-function-cognitive-complexity)

double AnalysisModule::calculate_disability_weight(const Person &entity) const {
    auto sum = 1.0;
    for (const auto &disease : entity.diseases) {
        if (disease.second.status == DiseaseStatus::active) {
            if (definition_.disability_weights().contains(disease.first)) {
                sum *= (1.0 - definition_.disability_weights().at(disease.first));
            }
        }
    }

    auto residual_dw = residual_disability_weight_.at(entity.age, entity.gender);
    residual_dw = std::min(1.0, std::max(residual_dw, 0.0));
    sum *= (1.0 - residual_dw);
    return 1.0 - sum;
}

DALYsIndicator AnalysisModule::calculate_dalys(Population &population, unsigned int max_age,
                                               unsigned int death_year) const {
    auto yll_sum = 0.0;
    auto yld_sum = 0.0;
    auto count = 0.0;
    for (const auto &entity : population) {
        if (entity.time_of_death() == death_year && entity.age <= max_age) {
            auto male_reference_age =
                definition_.life_expectancy().at(death_year, core::Gender::male);
            auto female_reference_age =
                definition_.life_expectancy().at(death_year, core::Gender::female);

            auto reference_age = std::max(male_reference_age, female_reference_age);
            auto lifeExpectancy = std::max(reference_age - entity.age, 0.0f);
            yll_sum += lifeExpectancy;
        }

        if (entity.is_active()) {
            yld_sum += calculate_disability_weight(entity);
            count++;
        }
    }

    auto yll = yll_sum * DALY_UNITS / count;
    auto yld = yld_sum * DALY_UNITS / count;
    return DALYsIndicator{.years_of_life_lost = yll,
                          .years_lived_with_disability = yld,
                          .disability_adjusted_life_years = yll + yld};
}

void AnalysisModule::update_death_and_migration_stats(const Person &person, size_t index,
                                                      RuntimeContext &context) {
    auto current_time = static_cast<unsigned int>(context.time_now());

    if (!person.is_alive() && person.time_of_death() == context.time_now()) {
        calculated_stats_[index + channel_index_.at("deaths")]++;
        float expected_life = definition_.life_expectancy().at(context.time_now(), person.gender);
        double yll = std::max(expected_life - person.age, 0.0f) * DALY_UNITS;
        calculated_stats_[index + channel_index_.at("mean_yll")] += yll;
        calculated_stats_[index + channel_index_.at("mean_daly")] += yll;
    }

    if (person.has_emigrated() && person.time_of_migration() == context.time_now()) {
        calculated_stats_[index + channel_index_.at("emigrations")]++;
    }
}

void AnalysisModule::update_calculated_stats_for_person(RuntimeContext &context,
                                                        const Person &person, size_t index) {
    calculated_stats_[index + channel_index_.at("count")]++;

    for (const auto &factor : context.mapping().entries()) {
        double value = person.get_risk_factor_value(factor.key());
        calculated_stats_[index + channel_index_.at("mean_" + factor.key().to_string())] += value;
    }

    for (const auto &[disease_name, disease_state] : person.diseases) {
        if (disease_state.status == DiseaseStatus::active) {
            calculated_stats_[index +
                              channel_index_.at("prevalence_" + disease_name.to_string())]++;
            if (disease_state.start_time == context.time_now()) {
                calculated_stats_[index +
                                  channel_index_.at("incidence_" + disease_name.to_string())]++;
            }
        }
    }
}

void AnalysisModule::calculate_population_statistics(RuntimeContext &context) {

    for (const auto &person : context.population()) {
        // First let's fetch the correct `calculated_stats_` bin index for this person
        size_t index = calculate_index(person);

        if (!person.is_active()) {
            update_death_and_migration_stats(person, index, context);
            continue;
        }

        update_calculated_stats_for_person(context, person, index);

        double dw = calculate_disability_weight(person);
        double yld = dw * DALY_UNITS;
        calculated_stats_[index + channel_index_.at("mean_yld")] += yld;
        calculated_stats_[index + channel_index_.at("mean_daly")] += yld;

        classify_weight(person);
    }

    // For each bin in the calculated stats...
    for (size_t i = 0; i < calculated_stats_.size(); i += channels_.size()) {
        double count_F = calculated_stats_[i + channel_index_.at("count")];
        double count_M = calculated_stats_[i + channel_index_.at("count")];
        double deaths_F = calculated_stats_[i + channel_index_.at("deaths")];
        double deaths_M = calculated_stats_[i + channel_index_.at("deaths")];

        // Calculate in-place factor averages.
        for (const auto &factor : context.mapping().entries()) {
            calculated_stats_[i + channel_index_.at("mean_" + factor.key().to_string())] /= count_F;
            calculated_stats_[i + channel_index_.at("mean_" + factor.key().to_string())] /= count_M;
        }

        // Calculate in-place disease prevalence and incidence rates.
        for (const auto &disease : context.diseases()) {
            calculated_stats_[i + channel_index_.at("prevalence_" + disease.code.to_string())] /=
                count_F;
            calculated_stats_[i + channel_index_.at("prevalence_" + disease.code.to_string())] /=
                count_M;
            calculated_stats_[i + channel_index_.at("incidence_" + disease.code.to_string())] /=
                count_F;
            calculated_stats_[i + channel_index_.at("incidence_" + disease.code.to_string())] /=
                count_M;
        }

        // Calculate in-place YLL/YLD/DALY averages.
        for (const auto &column : {"mean_yll", "mean_yld", "mean_daly"}) {
            calculated_stats_[i + channel_index_.at(column)] /= (count_F + deaths_F);
            calculated_stats_[i + channel_index_.at(column)] /= (count_M + deaths_M);
        }
    }
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
void AnalysisModule::calculate_population_statistics(RuntimeContext &context,
                                                     DataSeries &series) const {
    if (series.size() > 0) {
        throw std::logic_error("This should be a new object!");
    }

    series.add_channels(channels_);

    auto current_time = static_cast<unsigned int>(context.time_now());
    for (const auto &person : context.population()) {
        auto age = person.age;
        auto gender = person.gender;

        if (!person.is_active()) {
            if (!person.is_alive() && person.time_of_death() == current_time) {
                series(gender, "deaths").at(age)++;
                float expcted_life = definition_.life_expectancy().at(context.time_now(), gender);
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
            series(gender, "mean_" + factor.key().to_string()).at(age) +=
                person.get_risk_factor_value(factor.key());
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
            series(core::Gender::female, column).at(age) /= count_F;
            series(core::Gender::male, column).at(age) /= count_M;
        }

        // Calculate in-place disease prevalence and incidence rates.
        for (const auto &disease : context.diseases()) {
            std::string column_prevalence = "prevalence_" + disease.code.to_string();
            series(core::Gender::female, column_prevalence).at(age) /= count_F;
            series(core::Gender::male, column_prevalence).at(age) /= count_M;
            std::string column_incidence = "incidence_" + disease.code.to_string();
            series(core::Gender::female, column_incidence).at(age) /= count_F;
            series(core::Gender::male, column_incidence).at(age) /= count_M;
        }

        // Calculate in-place YLL/YLD/DALY averages.
        for (const auto &column : {"mean_yll", "mean_yld", "mean_daly"}) {
            series(core::Gender::female, column).at(age) /= (count_F + deaths_F);
            series(core::Gender::male, column).at(age) /= (count_M + deaths_M);
        }
    }

    // Calculate standard deviation
    calculate_standard_deviation(context, series);
}
// NOLINTEND(readability-function-cognitive-complexity)

void AnalysisModule::calculate_standard_deviation(RuntimeContext &context,
                                                  DataSeries &series) const {

    // Accumulate squared deviations from mean.
    auto accumulate_squared_diffs = [&series](const std::string &chan, core::Gender sex, int age,
                                              double value) {
        const double mean = series(sex, "mean_" + chan).at(age);
        const double diff = value - mean;
        series(sex, "std_" + chan).at(age) += diff * diff;
    };

    auto current_time = static_cast<unsigned int>(context.time_now());
    for (const auto &person : context.population()) {
        unsigned int age = person.age;
        core::Gender sex = person.gender;

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

void AnalysisModule::classify_weight(const Person &person) {
    auto weight_class = weight_classifier_.classify_weight(person);
    switch (weight_class) {
    case WeightCategory::normal:
        calculated_stats_[channel_index_.at("normal_weight")]++;
        break;
    case WeightCategory::overweight:
        calculated_stats_[channel_index_.at("over_weight")]++;
        calculated_stats_[channel_index_.at("above_weight")]++;
        break;
    case WeightCategory::obese:
        calculated_stats_[channel_index_.at("obese_weight")]++;
        calculated_stats_[channel_index_.at("above_weight")]++;
        break;
    default:
        throw std::logic_error("Unknown weight classification category.");
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

    // Since we will be performing frequent lookups, we will store the strings and indexes in a map
    // for quick access.
    for (size_t i = 0; i < channels_.size(); i++) {
        channel_index_.emplace(channels_[i], i);
    }
}

size_t AnalysisModule::calculate_index(const Person &person) const {
    // Get the bin index for each factor
    std::vector<size_t> bin_indices;
    for (size_t i = 0; i < factors_to_calculate_.size(); i++) {
        double factor_value = person.get_risk_factor_value(factors_to_calculate_[i]);
        auto bin_index =
            static_cast<size_t>((factor_value - factor_min_values_[i]) / factor_bin_widths_[i]);
        bin_indices.push_back(bin_index);
    }

    // Calculate the index in the calculated_stats_ vector
    size_t index = 0;
    for (size_t i = 0; i < bin_indices.size() - 1; i++) {
        size_t accumulated_bins =
            std::accumulate(std::next(factor_bins_.cbegin(), i + 1), factor_bins_.cend(), size_t{1},
                            std::multiplies<>());
        index += bin_indices[i] * accumulated_bins * channels_.size();
    }
    index += bin_indices.back() * channels_.size();

    return index;
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
