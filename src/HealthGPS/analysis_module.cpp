
#include "HealthGPS.Core/thread_util.h"

#include "analysis_module.h"
#include "converter.h"
#include "lms_model.h"
#include "weight_model.h"

#include <cmath>
#include <functional>
#include <future>
#include <iostream> // Added for debug prints
#include <oneapi/tbb/parallel_for_each.h>
#include <unordered_set>

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

        factor_min_values_.emplace_back(min_factor);

        // The number of bins to use for each factor is the number of integer values of the factor,
        // or 100 bins of equal size, whichever is smaller (100 is an arbitrary number, it could be
        // any other number depending on the desired resolution of the map)
        factor_bins_.emplace_back(std::min(100, static_cast<int>(max_factor - min_factor)));

        // The width of each bin is the range of the factor divided by the number of bins
        factor_bin_widths_.emplace_back((max_factor - min_factor) / factor_bins_.back());
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

void AnalysisModule::set_income_analysis_enabled(bool enabled) noexcept {
    // Set the income analysis flag - forces recompilation
    enable_income_analysis_ = enabled;
}

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

    // Add income-based analysis if enabled
    if (enable_income_analysis_) {
        calculate_income_based_statistics(context, result);
    }
}
// NOLINTEND(readability-function-cognitive-complexity)

void AnalysisModule::calculate_income_based_statistics(RuntimeContext &context,
                                                       ModelResult &result) const {
    auto available_income_categories = get_available_income_categories(context);

    // Initialize income-based maps
    auto risk_factors_by_income =
        std::map<core::Identifier, std::map<core::Income, std::map<core::Gender, double>>>();
    auto prevalence_by_income =
        std::map<core::Identifier, std::map<core::Income, std::map<core::Gender, int>>>();
    auto comorbidity_by_income = std::map<unsigned int, std::map<core::Income, ResultByGender>>();
    auto population_by_income = std::map<core::Income, int>();

    // Aggregate statistics by income category
    for (const auto &person : context.population()) {
        if (!person.is_active())
            continue;

        auto income = person.income;
        population_by_income[income]++;

        // Risk factors by income
        for (const auto &factor : context.mapping().entries()) {
            if (factor.level() > 0) {
                risk_factors_by_income[factor.key()][income][person.gender] +=
                    person.get_risk_factor_value(factor.key());
            }
        }

        // Disease prevalence by income
        for (const auto &[disease_name, disease_state] : person.diseases) {
            if (disease_state.status == DiseaseStatus::active) {
                prevalence_by_income[disease_name][income][person.gender]++;
            }
        }

        // Comorbidity by income
        auto comorbidity_count =
            std::min(static_cast<unsigned int>(person.diseases.size()), comorbidities_);
        if (person.gender == core::Gender::male) {
            comorbidity_by_income[comorbidity_count][income].male++;
        } else {
            comorbidity_by_income[comorbidity_count][income].female++;
        }
    }

    // Calculate averages and populate result
    result.population_by_income = ResultByIncome{};
    result.risk_factor_average_by_income = std::map<std::string, ResultByIncomeGender>{};
    result.disease_prevalence_by_income = std::map<std::string, ResultByIncomeGender>{};
    result.comorbidity_by_income = std::map<unsigned int, ResultByIncomeGender>{};

    // Populate population counts
    for (const auto &[income, count] : population_by_income) {
        switch (income) {
        case core::Income::low:
            result.population_by_income->low = count;
            break;
        case core::Income::lowermiddle:
            result.population_by_income->lowermiddle = count;
            break;
        case core::Income::middle:
            result.population_by_income->middle = count;
            break;
        case core::Income::uppermiddle:
            result.population_by_income->uppermiddle = count;
            break;
        case core::Income::high:
            result.population_by_income->high = count;
            break;
        default:
            break;
        }
    }

    // Calculate risk factor averages by income
    for (const auto &[factor_key, income_data] : risk_factors_by_income) {
        auto factor_name = context.mapping().at(factor_key).name();
        auto result_by_income = ResultByIncomeGender{};

        for (const auto &[income, gender_data] : income_data) {
            auto count = population_by_income[income];
            if (count > 0) {
                double male_avg = 0.0;
                double female_avg = 0.0;

                // Use .at() for const map access
                auto male_it = gender_data.find(core::Gender::male);
                if (male_it != gender_data.end()) {
                    male_avg = male_it->second / count;
                }

                auto female_it = gender_data.find(core::Gender::female);
                if (female_it != gender_data.end()) {
                    female_avg = female_it->second / count;
                }

                switch (income) {
                case core::Income::low:
                    result_by_income.low = ResultByGender{male_avg, female_avg};
                    break;
                case core::Income::lowermiddle:
                    result_by_income.lowermiddle =
                        ResultByGender{.male = male_avg, .female = female_avg};
                    break;
                case core::Income::middle:
                    result_by_income.middle =
                        ResultByGender{.male = male_avg, .female = female_avg};
                    break;
                case core::Income::uppermiddle:
                    result_by_income.uppermiddle =
                        ResultByGender{.male = male_avg, .female = female_avg};
                    break;
                case core::Income::high:
                    result_by_income.high = ResultByGender{male_avg, female_avg};
                    break;
                default:
                    break;
                }
            }
        }
        result.risk_factor_average_by_income->emplace(factor_name, result_by_income);
    }

    // Calculate disease prevalence by income
    for (const auto &[disease_key, income_data] : prevalence_by_income) {
        auto disease_name = disease_key.to_string();
        auto result_by_income = ResultByIncomeGender{};

        for (const auto &[income, gender_data] : income_data) {
            auto count = population_by_income[income];
            if (count > 0) {
                double male_prevalence = 0.0;
                double female_prevalence = 0.0;

                // Use .at() for const map access
                auto male_it = gender_data.find(core::Gender::male);
                if (male_it != gender_data.end()) {
                    male_prevalence = male_it->second * 100.0 / count;
                }

                auto female_it = gender_data.find(core::Gender::female);
                if (female_it != gender_data.end()) {
                    female_prevalence = female_it->second * 100.0 / count;
                }

                switch (income) {
                case core::Income::low:
                    result_by_income.low = ResultByGender{male_prevalence, female_prevalence};
                    break;
                case core::Income::lowermiddle:
                    result_by_income.lowermiddle =
                        ResultByGender{.male = male_prevalence, .female = female_prevalence};
                    break;
                case core::Income::middle:
                    result_by_income.middle =
                        ResultByGender{.male = male_prevalence, .female = female_prevalence};
                    break;
                case core::Income::uppermiddle:
                    result_by_income.uppermiddle =
                        ResultByGender{.male = male_prevalence, .female = female_prevalence};
                    break;
                case core::Income::high:
                    result_by_income.high =
                        ResultByGender{.male = male_prevalence, .female = female_prevalence};
                    break;
                default:
                    break;
                }
            }
        }
        result.disease_prevalence_by_income->emplace(disease_name, result_by_income);
    }

    // Calculate comorbidity by income
    for (const auto &[comorbidity_count, income_data] : comorbidity_by_income) {
        auto result_by_income = ResultByIncomeGender{};

        for (const auto &[income, gender_data] : income_data) {
            auto count = population_by_income[income];
            if (count > 0) {
                double male_comorbidity = gender_data.male * 100.0 / count;
                double female_comorbidity = gender_data.female * 100.0 / count;

                switch (income) {
                case core::Income::low:
                    result_by_income.low = ResultByGender{male_comorbidity, female_comorbidity};
                    break;
                case core::Income::lowermiddle:
                    result_by_income.lowermiddle =
                        ResultByGender{.male = male_comorbidity, .female = female_comorbidity};
                    break;
                case core::Income::middle:
                    result_by_income.middle =
                        ResultByGender{.male = male_comorbidity, .female = female_comorbidity};
                    break;
                case core::Income::uppermiddle:
                    result_by_income.uppermiddle =
                        ResultByGender{.male = male_comorbidity, .female = female_comorbidity};
                    break;
                case core::Income::high:
                    result_by_income.high = ResultByGender{male_comorbidity, female_comorbidity};
                    break;
                default:
                    break;
                }
            }
        }
        result.comorbidity_by_income->emplace(comorbidity_count, result_by_income);
    }
}

std::vector<core::Income>
AnalysisModule::get_available_income_categories(RuntimeContext &context) const {
    std::vector<core::Income> categories;
    std::unordered_set<core::Income> seen;

    for (const auto &person : context.population()) {
        if (person.is_active() && !seen.contains(person.income)) {
            categories.emplace_back(person.income);
            seen.insert(person.income);
        }
    }
    return categories;
}

std::string AnalysisModule::income_category_to_string(core::Income income) const {
    switch (income) {
    case core::Income::low:
        return "LowIncome";
    case core::Income::lowermiddle:
        return "LowerMiddleIncome";
    case core::Income::middle:
        return "MiddleIncome";
    case core::Income::uppermiddle:
        return "UpperMiddleIncome";
    case core::Income::high:
        return "HighIncome";
    case core::Income::unknown:
        return "UnknownIncome";
    default:
        return "UnknownIncome";
    }
}

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

void AnalysisModule::calculate_population_statistics(RuntimeContext &context) {
    size_t num_factors_to_calculate =
        context.mapping().entries().size() - factors_to_calculate_.size();

    for (const auto &person : context.population()) {
        // Get the bin index for each factor
        std::vector<size_t> bin_indices;
        for (size_t i = 0; i < factors_to_calculate_.size(); i++) {
            double factor_value = person.get_risk_factor_value(factors_to_calculate_[i]);
            auto bin_index =
                static_cast<size_t>((factor_value - factor_min_values_[i]) / factor_bin_widths_[i]);
            bin_indices.emplace_back(bin_index);
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
            if (std::ranges::find(factors_to_calculate_, factor.key()) ==
                factors_to_calculate_.end()) {
                calculated_stats_[index++] += person.get_risk_factor_value(factor.key());
            }
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

        // NEW: Collect age data (always available)
        series(gender, "mean_age").at(age) += person.age;
        series(gender, "mean_age2").at(age) += (person.age * person.age);
        series(gender, "mean_age3").at(age) += (person.age * person.age * person.age);

        // NEW: Collect demographic data (only if available)
        // Region data - check if person has region assigned
        if (!person.region.empty() && person.region != "unknown") {
            series(gender, "mean_region").at(age) += person.region_to_value();
        }

        // Ethnicity data - check if person has ethnicity assigned
        if (!person.ethnicity.empty() && person.ethnicity != "unknown") {
            series(gender, "mean_ethnicity").at(age) += person.ethnicity_to_value();
        }

        // Sector data - check if person has sector assigned
        if (person.sector != core::Sector::unknown) {
            series(gender, "mean_sector").at(age) += person.sector_to_value();
        }

        // NEW: Collect income data (only if available)
        // Income category - check if person has income assigned
        if (person.income != core::Income::unknown) {
            series(gender, "mean_income_category").at(age) += person.income_to_value();
        }

        // Continuous income - check if available in risk factors
        auto income_continuous = 0.0;
        auto it = person.risk_factors.find("income_continuous"_id);
        if (it != person.risk_factors.end()) {
            income_continuous = it->second;
            series(gender, "mean_income_continuous").at(age) += income_continuous;
        }

        // NEW: Collect physical activity data (only if available)
        auto physical_activity = 0.0;
        auto pa_it = person.risk_factors.find("PhysicalActivity"_id);
        if (pa_it != person.risk_factors.end()) {
            physical_activity = pa_it->second;
            series(gender, "mean_physical_activity").at(age) += physical_activity;
        }

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
            if (count_F > 0) {
                series(core::Gender::female, column).at(age) /= count_F;
            }
            if (count_M > 0) {
                series(core::Gender::male, column).at(age) /= count_M;
            }
        }

        // NEW: Calculate in-place age averages (always available)
        if (count_F > 0) {
            series(core::Gender::female, "mean_age").at(age) /= count_F;
            series(core::Gender::female, "mean_age2").at(age) /= count_F;
            series(core::Gender::female, "mean_age3").at(age) /= count_F;
        }
        if (count_M > 0) {
            series(core::Gender::male, "mean_age").at(age) /= count_M;
            series(core::Gender::male, "mean_age2").at(age) /= count_M;
            series(core::Gender::male, "mean_age3").at(age) /= count_M;
        }

        // MAHIMA: Calculate in-place demographic averages (only if data exists)
        // Region averages
        if (count_F > 0) {
            series(core::Gender::female, "mean_region").at(age) /= count_F;
        }
        if (count_M > 0) {
            series(core::Gender::male, "mean_region").at(age) /= count_M;
        }

        // Ethnicity averages
        if (count_F > 0) {
            series(core::Gender::female, "mean_ethnicity").at(age) /= count_F;
        }
        if (count_M > 0) {
            series(core::Gender::male, "mean_ethnicity").at(age) /= count_M;
        }

        // Sector averages
        if (count_F > 0) {
            series(core::Gender::female, "mean_sector").at(age) /= count_F;
        }
        if (count_M > 0) {
            series(core::Gender::male, "mean_sector").at(age) /= count_M;
        }

        // Income category averages
        if (count_F > 0) {
            series(core::Gender::female, "mean_income_category").at(age) /= count_F;
        }
        if (count_M > 0) {
            series(core::Gender::male, "mean_income_category").at(age) /= count_M;
        }

        // Income continuous averages
        if (count_F > 0) {
            series(core::Gender::female, "mean_income_continuous").at(age) /= count_F;
        }
        if (count_M > 0) {
            series(core::Gender::male, "mean_income_continuous").at(age) /= count_M;
        }

        // Physical activity averages
        if (count_F > 0) {
            series(core::Gender::female, "mean_physical_activity").at(age) /= count_F;
        }
        if (count_M > 0) {
            series(core::Gender::male, "mean_physical_activity").at(age) /= count_M;
        }

        // Calculate in-place disease prevalence and incidence rates.
        for (const auto &disease : context.diseases()) {
            std::string column_prevalence = "prevalence_" + disease.code.to_string();
            std::string column_incidence = "incidence_" + disease.code.to_string();
            if (count_F > 0) {
                series(core::Gender::female, column_prevalence).at(age) /= count_F;
                series(core::Gender::female, column_incidence).at(age) /= count_F;
            }
            if (count_M > 0) {
                series(core::Gender::male, column_prevalence).at(age) /= count_M;
                series(core::Gender::male, column_incidence).at(age) /= count_M;
            }
        }

        // Calculate in-place YLL/YLD/DALY averages.
        for (const auto &column : {"mean_yll", "mean_yld", "mean_daly"}) {
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

    // Add income-based analysis if enabled
    if (enable_income_analysis_) {
        calculate_income_based_population_statistics(context, series);
    }
}
// NOLINTEND(readability-function-cognitive-complexity)

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void AnalysisModule::calculate_income_based_population_statistics(RuntimeContext &context,
                                                                  DataSeries &series) const {
    if (!enable_income_analysis_) {
        return;
    }

    auto available_income_categories = get_available_income_categories(context);

    // Create a list of only the channels that are actually used in income-based analysis
    auto income_channels = std::vector<std::string>();
    income_channels.emplace_back("count");
    income_channels.emplace_back("deaths");
    income_channels.emplace_back("emigrations");
    income_channels.emplace_back("mean_yll");
    income_channels.emplace_back("mean_yld");
    income_channels.emplace_back("mean_daly");
    income_channels.emplace_back("normal_weight");
    income_channels.emplace_back("over_weight");
    income_channels.emplace_back("obese_weight");
    income_channels.emplace_back("above_weight");

    // Add demographic channels (excluding mean_age, std_age, mean_gender, std_gender)
    // income_channels.emplace_back("mean_age");
    // income_channels.emplace_back("std_age");
    // income_channels.emplace_back("mean_gender");
    // income_channels.emplace_back("std_gender");
    income_channels.emplace_back("mean_income");
    income_channels.emplace_back("std_income");
    income_channels.emplace_back("mean_sector");
    income_channels.emplace_back("std_sector");

    // MAHIMA: Add enhanced demographic channels for income analysis (only if data exists)
    // These will be populated if region, ethnicity data is available
    income_channels.emplace_back("mean_region");
    income_channels.emplace_back("std_region");
    income_channels.emplace_back("mean_ethnicity");
    income_channels.emplace_back("std_ethnicity");

    // MAHIMA: Add enhanced income channels for income analysis (only if data exists)
    // These will be populated if income data is available
    income_channels.emplace_back("mean_income_category");
    income_channels.emplace_back("std_income_category");
    income_channels.emplace_back("mean_income_continuous");
    income_channels.emplace_back("std_income_continuous");

    // MAHIMA: Add physical activity channels for income analysis (only if data exists)
    // These will be populated if physical activity data is available
    income_channels.emplace_back("mean_physical_activity");
    income_channels.emplace_back("std_physical_activity");

    // Add risk factor channels
    for (const auto &factor : context.mapping().entries()) {
        income_channels.emplace_back("mean_" + factor.key().to_string());
        income_channels.emplace_back("std_" + factor.key().to_string());
    }

    // Add disease prevalence and incidence channels
    for (const auto &disease : context.diseases()) {
        income_channels.emplace_back("prevalence_" + disease.code.to_string());
        income_channels.emplace_back("incidence_" + disease.code.to_string());
    }

    // Add YLL/YLD/DALY std channels
    income_channels.emplace_back("std_yll");
    income_channels.emplace_back("std_yld");
    income_channels.emplace_back("std_daly");

    // Create income channels for the actual income categories found in the data
    std::vector<core::Income> actual_income_categories;
    for (const auto &person : context.population()) {
        if (std::ranges::find(actual_income_categories, person.income) ==
            actual_income_categories.end()) {
            actual_income_categories.emplace_back(person.income);
        }
    }

    // Create income channels for the actual categories found
    series.add_income_channels_for_categories(income_channels, actual_income_categories);

    // Initialize standard deviation channels with zeros
    const auto age_range = context.age_range();
    for (const auto &income : actual_income_categories) {
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            for (const auto &factor : context.mapping().entries()) {
                series.at(core::Gender::female, income, "std_" + factor.key().to_string()).at(age) =
                    0.0;
                series.at(core::Gender::male, income, "std_" + factor.key().to_string()).at(age) =
                    0.0;
            }
            for (const auto &column : {"std_yll", "std_yld", "std_daly"}) {
                series.at(core::Gender::female, income, column).at(age) = 0.0;
                series.at(core::Gender::male, income, column).at(age) = 0.0;
            }
            for (const auto &column : {"std_income", "std_sector"}) {
                series.at(core::Gender::female, income, column).at(age) = 0.0;
                series.at(core::Gender::male, income, column).at(age) = 0.0;
            }

            // NEW: Initialize age standard deviation channels with zeros (always available)
            for (const auto &column : {"std_age2", "std_age3"}) {
                series.at(core::Gender::female, income, column).at(age) = 0.0;
                series.at(core::Gender::male, income, column).at(age) = 0.0;
            }

            // NEW: Initialize enhanced demographic standard deviation channels with zeros (only if
            // data exists)
            for (const auto &column : {"std_region", "std_ethnicity", "std_income_category",
                                       "std_income_continuous", "std_physical_activity"}) {
                series.at(core::Gender::female, income, column).at(age) = 0.0;
                series.at(core::Gender::male, income, column).at(age) = 0.0;
            }
        }
    }

    auto current_time = static_cast<unsigned int>(context.time_now());

    for (const auto &person : context.population()) {

        auto age = person.age;
        auto gender = person.gender;
        auto income = person.income;

        if (!person.is_active()) {
            if (!person.is_alive() && person.time_of_death() == current_time) {
                try {
                    series.at(gender, income, "deaths").at(age)++;
                    float expcted_life =
                        definition_.life_expectancy().at(context.time_now(), gender);
                    double yll = std::max(expcted_life - age, 0.0f) * DALY_UNITS;
                    series.at(gender, income, "mean_yll").at(age) += yll;
                    series.at(gender, income, "mean_daly").at(age) += yll;
                } catch (const std::exception &) {
                    throw;
                }
            }

            if (person.has_emigrated() && person.time_of_migration() == current_time) {
                try {
                    series.at(gender, income, "emigrations").at(age)++;
                } catch (const std::exception &) {
                    throw;
                }
            }

            continue;
        }

        try {
            series.at(gender, income, "count").at(age)++;
        } catch (const std::exception &) {
            throw;
        }

        for (const auto &factor : context.mapping().entries()) {
            auto channel_name = "mean_" + factor.key().to_string();
            try {
                series.at(gender, income, channel_name).at(age) +=
                    person.get_risk_factor_value(factor.key());
            } catch (const std::exception &) {
                throw;
            }
        }

        // Collect demographic data (excluding age and gender)
        try {
            // series.at(gender, income, "mean_age").at(age) += person.age;
            // series.at(gender, income, "mean_gender").at(age) += static_cast<int>(person.gender);
            series.at(gender, income, "mean_income").at(age) += static_cast<int>(person.income);
            series.at(gender, income, "mean_sector").at(age) += static_cast<int>(person.sector);
        } catch (const std::exception &) {
            throw;
        }

        // NEW: Collect enhanced demographic data by income (only if available)
        try {
            // Region data - check if person has region assigned
            if (!person.region.empty() && person.region != "unknown") {
                series.at(gender, income, "mean_region").at(age) += person.region_to_value();
            }

            // Ethnicity data - check if person has ethnicity assigned
            if (!person.ethnicity.empty() && person.ethnicity != "unknown") {
                series.at(gender, income, "mean_ethnicity").at(age) += person.ethnicity_to_value();
            }

            // Income category - check if person has income assigned
            if (person.income != core::Income::unknown) {
                series.at(gender, income, "mean_income_category").at(age) +=
                    person.income_to_value();
            }

            // Continuous income - check if available in risk factors
            auto income_continuous = 0.0;
            auto it = person.risk_factors.find("income_continuous"_id);
            if (it != person.risk_factors.end()) {
                income_continuous = it->second;
                series.at(gender, income, "mean_income_continuous").at(age) += income_continuous;
            }

            // Physical activity - check if available in risk factors
            auto physical_activity = 0.0;
            auto pa_it = person.risk_factors.find("PhysicalActivity"_id);
            if (pa_it != person.risk_factors.end()) {
                physical_activity = pa_it->second;
                series.at(gender, income, "mean_physical_activity").at(age) += physical_activity;
            }
        } catch (const std::exception &) {
            throw;
        }

        for (const auto &[disease_name, disease_state] : person.diseases) {
            if (disease_state.status == DiseaseStatus::active) {
                auto prevalence_channel = "prevalence_" + disease_name.to_string();
                series.at(gender, income, prevalence_channel).at(age)++;
                if (disease_state.start_time == context.time_now()) {
                    auto incidence_channel = "incidence_" + disease_name.to_string();
                    series.at(gender, income, incidence_channel).at(age)++;
                }
            }
        }

        double dw = calculate_disability_weight(person);
        double yld = dw * DALY_UNITS;
        series.at(gender, income, "mean_yld").at(age) += yld;
        series.at(gender, income, "mean_daly").at(age) += yld;

        // Classify weight for income-based analysis
        auto weight_class = weight_classifier_.classify_weight(person);
        switch (weight_class) {
        case WeightCategory::normal:
            series.at(gender, income, "normal_weight").at(age)++;
            break;
        case WeightCategory::overweight:
            series.at(gender, income, "over_weight").at(age)++;
            series.at(gender, income, "above_weight").at(age)++;
            break;
        case WeightCategory::obese:
            series.at(gender, income, "obese_weight").at(age)++;
            series.at(gender, income, "above_weight").at(age)++;
            break;
        default:
            throw std::logic_error("Unknown weight classification category.");
            break;
        }
    }

    // Calculate averages for each income category

    for (const auto &income : available_income_categories) {
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            double count_F = series.at(core::Gender::female, income, "count").at(age);
            double count_M = series.at(core::Gender::male, income, "count").at(age);
            double deaths_F = series.at(core::Gender::female, income, "deaths").at(age);
            double deaths_M = series.at(core::Gender::male, income, "deaths").at(age);

            // Calculate in-place factor averages for this income category
            for (const auto &factor : context.mapping().entries()) {
                std::string column = "mean_" + factor.key().to_string();
                if (count_F > 0) {
                    series.at(core::Gender::female, income, column).at(age) /= count_F;
                }
                if (count_M > 0) {
                    series.at(core::Gender::male, income, column).at(age) /= count_M;
                }
            }

            // Calculate in-place demographic averages for this income category (excluding age and
            // gender)
            if (count_F > 0) {
                // series.at(core::Gender::female, income, "mean_age").at(age) /= count_F;
                // series.at(core::Gender::female, income, "mean_gender").at(age) /= count_F;
                series.at(core::Gender::female, income, "mean_income").at(age) /= count_F;
                series.at(core::Gender::female, income, "mean_sector").at(age) /= count_F;
            }
            if (count_M > 0) {
                // series.at(core::Gender::male, income, "mean_age").at(age) /= count_M;
                // series.at(core::Gender::male, income, "mean_gender").at(age) /= count_M;
                series.at(core::Gender::male, income, "mean_income").at(age) /= count_M;
                series.at(core::Gender::male, income, "mean_sector").at(age) /= count_M;
            }

            // MAHIMA: Calculate in-place enhanced demographic averages for this income category
            // (only if data exists)
            if (count_F > 0) {
                // Region averages
                series.at(core::Gender::female, income, "mean_region").at(age) /= count_F;
                // Ethnicity averages
                series.at(core::Gender::female, income, "mean_ethnicity").at(age) /= count_F;
                // Income category averages
                series.at(core::Gender::female, income, "mean_income_category").at(age) /= count_F;
                // Income continuous averages
                series.at(core::Gender::female, income, "mean_income_continuous").at(age) /=
                    count_F;
                // Physical activity averages
                series.at(core::Gender::female, income, "mean_physical_activity").at(age) /=
                    count_F;
            }
            if (count_M > 0) {
                // Region averages
                series.at(core::Gender::male, income, "mean_region").at(age) /= count_M;
                // Ethnicity averages
                series.at(core::Gender::male, income, "mean_ethnicity").at(age) /= count_M;
                // Income category averages
                series.at(core::Gender::male, income, "mean_income_category").at(age) /= count_M;
                // Income continuous averages
                series.at(core::Gender::male, income, "mean_income_continuous").at(age) /= count_M;
                // Physical activity averages
                series.at(core::Gender::male, income, "mean_physical_activity").at(age) /= count_M;
            }

            // Calculate in-place disease prevalence and incidence rates for this income category
            for (const auto &disease : context.diseases()) {
                std::string column_prevalence = "prevalence_" + disease.code.to_string();
                if (count_F > 0) {
                    series.at(core::Gender::female, income, column_prevalence).at(age) /= count_F;
                }
                if (count_M > 0) {
                    series.at(core::Gender::male, income, column_prevalence).at(age) /= count_M;
                }

                std::string column_incidence = "incidence_" + disease.code.to_string();
                if (count_F > 0) {
                    series.at(core::Gender::female, income, column_incidence).at(age) /= count_F;
                }
                if (count_M > 0) {
                    series.at(core::Gender::male, income, column_incidence).at(age) /= count_M;
                }
            }

            // Calculate in-place YLL/YLD/DALY averages for this income category
            for (const auto &column : {"mean_yll", "mean_yld", "mean_daly"}) {
                if (count_F + deaths_F > 0) {
                    series.at(core::Gender::female, income, column).at(age) /= (count_F + deaths_F);
                }
                if (count_M + deaths_M > 0) {
                    series.at(core::Gender::male, income, column).at(age) /= (count_M + deaths_M);
                }
            }
        }
    }

    // Calculate standard deviation for income-based data
    calculate_income_based_standard_deviation(context, series);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void AnalysisModule::calculate_income_based_standard_deviation(RuntimeContext &context,
                                                               DataSeries &series) const {
    // Accumulate squared deviations from mean for income-based data
    auto accumulate_squared_diffs_income = [&series](const std::string &chan, core::Gender sex,
                                                     core::Income income, int age, double value) {
        const double mean = series.at(sex, income, "mean_" + chan).at(age);
        const double diff = value - mean;
        series.at(sex, income, "std_" + chan).at(age) += diff * diff;
    };

    auto current_time = static_cast<unsigned int>(context.time_now());
    for (const auto &person : context.population()) {
        unsigned int age = person.age;
        core::Gender sex = person.gender;
        core::Income income = person.income;

        if (!person.is_active()) {
            if (!person.is_alive() && person.time_of_death() == current_time) {
                float expcted_life = definition_.life_expectancy().at(context.time_now(), sex);
                double yll = std::max(expcted_life - age, 0.0f) * DALY_UNITS;
                accumulate_squared_diffs_income("yll", sex, income, age, yll);
                accumulate_squared_diffs_income("daly", sex, income, age, yll);
            }
            continue;
        }

        double dw = calculate_disability_weight(person);
        double yld = dw * DALY_UNITS;
        accumulate_squared_diffs_income("yld", sex, income, age, yld);
        accumulate_squared_diffs_income("daly", sex, income, age, yld);

        for (const auto &factor : context.mapping().entries()) {
            const double value = person.get_risk_factor_value(factor.key());
            accumulate_squared_diffs_income(factor.key().to_string(), sex, income, age, value);
        }

        // Accumulate squared deviations for demographic data (excluding age and gender)
        // accumulate_squared_diffs_income("age", sex, income, age, person.age);
        // accumulate_squared_diffs_income("gender", sex, income, age,
        // static_cast<int>(person.gender));
        accumulate_squared_diffs_income("income", sex, income, age,
                                        static_cast<int>(person.income));
        accumulate_squared_diffs_income("sector", sex, income, age,
                                        static_cast<int>(person.sector));

        // NEW: Accumulate squared deviations for enhanced demographic data (only if available)
        // Region standard deviation
        if (!person.region.empty() && person.region != "unknown") {
            const double value = person.region_to_value();
            accumulate_squared_diffs_income("region", sex, income, age, value);
        }

        // Ethnicity standard deviation
        if (!person.ethnicity.empty() && person.ethnicity != "unknown") {
            const double value = person.ethnicity_to_value();
            accumulate_squared_diffs_income("ethnicity", sex, income, age, value);
        }

        // Income category standard deviation
        if (person.income != core::Income::unknown) {
            const double value = person.income_to_value();
            accumulate_squared_diffs_income("income_category", sex, income, age, value);
        }

        // Income continuous standard deviation
        auto it = person.risk_factors.find("income_continuous"_id);
        if (it != person.risk_factors.end()) {
            const double value = it->second;
            accumulate_squared_diffs_income("income_continuous", sex, income, age, value);
        }

        // Physical activity standard deviation
        auto pa_it = person.risk_factors.find("PhysicalActivity"_id);
        if (pa_it != person.risk_factors.end()) {
            const double value = pa_it->second;
            accumulate_squared_diffs_income("physical_activity", sex, income, age, value);
        }
    }

    // Calculate in-place standard deviation for income-based data
    auto divide_by_count_sqrt_income = [&series](const std::string &chan, core::Gender sex,
                                                 core::Income income, int age, double count) {
        if (count > 0) {
            const double sum = series.at(sex, income, "std_" + chan).at(age);
            const double std = std::sqrt(sum / count);
            series.at(sex, income, "std_" + chan).at(age) = std;
        } else {
            series.at(sex, income, "std_" + chan).at(age) = 0.0;
        }
    };

    // For each income category and age group
    const auto age_range = context.age_range();
    auto available_income_categories = get_available_income_categories(context);

    for (const auto &income : available_income_categories) {
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            double count_F = series.at(core::Gender::female, income, "count").at(age);
            double count_M = series.at(core::Gender::male, income, "count").at(age);
            double deaths_F = series.at(core::Gender::female, income, "deaths").at(age);
            double deaths_M = series.at(core::Gender::male, income, "deaths").at(age);

            // Calculate in-place factor standard deviation for this income category
            for (const auto &factor : context.mapping().entries()) {
                divide_by_count_sqrt_income(factor.key().to_string(), core::Gender::female, income,
                                            age, count_F);
                divide_by_count_sqrt_income(factor.key().to_string(), core::Gender::male, income,
                                            age, count_M);
            }

            // Calculate in-place demographic standard deviation for this income category (excluding
            // age and gender) divide_by_count_sqrt_income("age", core::Gender::female, income, age,
            // count_F); divide_by_count_sqrt_income("age", core::Gender::male, income, age,
            // count_M); divide_by_count_sqrt_income("gender", core::Gender::female, income, age,
            // count_F); divide_by_count_sqrt_income("gender", core::Gender::male, income, age,
            // count_M);
            divide_by_count_sqrt_income("income", core::Gender::female, income, age, count_F);
            divide_by_count_sqrt_income("income", core::Gender::male, income, age, count_M);
            divide_by_count_sqrt_income("sector", core::Gender::female, income, age, count_F);
            divide_by_count_sqrt_income("sector", core::Gender::male, income, age, count_M);

            // NEW: Calculate in-place enhanced demographic standard deviation for this income
            // category (only if data exists) Region standard deviation
            divide_by_count_sqrt_income("region", core::Gender::female, income, age, count_F);
            divide_by_count_sqrt_income("region", core::Gender::male, income, age, count_M);

            // Ethnicity standard deviation
            divide_by_count_sqrt_income("ethnicity", core::Gender::female, income, age, count_F);
            divide_by_count_sqrt_income("ethnicity", core::Gender::male, income, age, count_M);

            // Income category standard deviation
            divide_by_count_sqrt_income("income_category", core::Gender::female, income, age,
                                        count_F);
            divide_by_count_sqrt_income("income_category", core::Gender::male, income, age,
                                        count_M);

            // Income continuous standard deviation
            divide_by_count_sqrt_income("income_continuous", core::Gender::female, income, age,
                                        count_F);
            divide_by_count_sqrt_income("income_continuous", core::Gender::male, income, age,
                                        count_M);

            // Physical activity standard deviation
            divide_by_count_sqrt_income("physical_activity", core::Gender::female, income, age,
                                        count_F);
            divide_by_count_sqrt_income("physical_activity", core::Gender::male, income, age,
                                        count_M);

            // Calculate in-place YLL/YLD/DALY standard deviation for this income category
            for (const auto &column : {"yll", "yld", "daly"}) {
                divide_by_count_sqrt_income(column, core::Gender::female, income, age,
                                            (count_F + deaths_F));
                divide_by_count_sqrt_income(column, core::Gender::male, income, age,
                                            (count_M + deaths_M));
            }
        }
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
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

        // NEW: Calculate standard deviation for age factors (always available)
        accumulate_squared_diffs("age", sex, age, person.age);
        accumulate_squared_diffs("age2", sex, age, (person.age * person.age));
        accumulate_squared_diffs("age3", sex, age, (person.age * person.age * person.age));

        for (const auto &factor : context.mapping().entries()) {
            const double value = person.get_risk_factor_value(factor.key());
            accumulate_squared_diffs(factor.key().to_string(), sex, age, value);
        }

        // NEW: Calculate standard deviation for demographic factors (only if data exists)
        // Region standard deviation
        if (!person.region.empty() && person.region != "unknown") {
            const double value = person.region_to_value();
            accumulate_squared_diffs("region", sex, age, value);
        }

        // Ethnicity standard deviation
        if (!person.ethnicity.empty() && person.ethnicity != "unknown") {
            const double value = person.ethnicity_to_value();
            accumulate_squared_diffs("ethnicity", sex, age, value);
        }

        // Sector standard deviation
        if (person.sector != core::Sector::unknown) {
            const double value = person.sector_to_value();
            accumulate_squared_diffs("sector", sex, age, value);
        }

        // Income category standard deviation
        if (person.income != core::Income::unknown) {
            const double value = person.income_to_value();
            accumulate_squared_diffs("income_category", sex, age, value);
        }

        // Income continuous standard deviation
        auto it = person.risk_factors.find("income_continuous"_id);
        if (it != person.risk_factors.end()) {
            const double value = it->second;
            accumulate_squared_diffs("income_continuous", sex, age, value);
        }

        // Physical activity standard deviation
        auto pa_it = person.risk_factors.find("PhysicalActivity"_id);
        if (pa_it != person.risk_factors.end()) {
            const double value = pa_it->second;
            accumulate_squared_diffs("physical_activity", sex, age, value);
        }
    }

    // Calculate in-place standard deviation.
    auto divide_by_count_sqrt = [&series](const std::string &chan, core::Gender sex, int age,
                                          double count) {
        if (count > 0) {
            const double sum = series(sex, "std_" + chan).at(age);
            const double std = std::sqrt(sum / count);
            series(sex, "std_" + chan).at(age) = std;
        } else {
            series(sex, "std_" + chan).at(age) = 0.0;
        }
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

        // NEW: Calculate in-place age standard deviation (always available)
        divide_by_count_sqrt("age", core::Gender::female, age, count_F);
        divide_by_count_sqrt("age", core::Gender::male, age, count_M);
        divide_by_count_sqrt("age2", core::Gender::female, age, count_F);
        divide_by_count_sqrt("age2", core::Gender::male, age, count_M);
        divide_by_count_sqrt("age3", core::Gender::female, age, count_F);
        divide_by_count_sqrt("age3", core::Gender::male, age, count_M);

        // Calculate in-place YLL/YLD/DALY standard deviation.
        for (const auto &column : {"yll", "yld", "daly"}) {
            divide_by_count_sqrt(column, core::Gender::female, age, (count_F + deaths_F));
            divide_by_count_sqrt(column, core::Gender::male, age, (count_M + deaths_M));
        }

        // NEW: Calculate in-place demographic standard deviation (only if data exists)
        // Region standard deviation
        divide_by_count_sqrt("region", core::Gender::female, age, count_F);
        divide_by_count_sqrt("region", core::Gender::male, age, count_M);

        // Ethnicity standard deviation
        divide_by_count_sqrt("ethnicity", core::Gender::female, age, count_F);
        divide_by_count_sqrt("ethnicity", core::Gender::male, age, count_M);

        // Sector standard deviation
        divide_by_count_sqrt("sector", core::Gender::female, age, count_F);
        divide_by_count_sqrt("sector", core::Gender::male, age, count_M);

        // Income category standard deviation
        divide_by_count_sqrt("income_category", core::Gender::female, age, count_F);
        divide_by_count_sqrt("income_category", core::Gender::male, age, count_M);

        // Income continuous standard deviation
        divide_by_count_sqrt("income_continuous", core::Gender::female, age, count_F);
        divide_by_count_sqrt("income_continuous", core::Gender::male, age, count_M);

        // Physical activity standard deviation
        divide_by_count_sqrt("physical_activity", core::Gender::female, age, count_F);
        divide_by_count_sqrt("physical_activity", core::Gender::male, age, count_M);
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

void AnalysisModule::initialise_output_channels(RuntimeContext &context) {
    if (!channels_.empty()) {
        return;
    }

    channels_.emplace_back("count");
    channels_.emplace_back("deaths");
    channels_.emplace_back("emigrations");

    // NEW: Add age channels (always available)
    channels_.emplace_back("mean_age");
    channels_.emplace_back("std_age");
    channels_.emplace_back("mean_age2");
    channels_.emplace_back("std_age2");
    channels_.emplace_back("mean_age3");
    channels_.emplace_back("std_age3");

    // NEW: Add demographic channels (only if data exists)
    // These will be populated if region, ethnicity, sector data is available
    channels_.emplace_back("mean_region");
    channels_.emplace_back("std_region");
    channels_.emplace_back("mean_ethnicity");
    channels_.emplace_back("std_ethnicity");
    channels_.emplace_back("mean_sector");
    channels_.emplace_back("std_sector");

    // NEW: Add income channels (only if data exists)
    // These will be populated if income data is available
    channels_.emplace_back("mean_income_category");
    channels_.emplace_back("std_income_category");
    channels_.emplace_back("mean_income_continuous");
    channels_.emplace_back("std_income_continuous");

    // NEW: Add physical activity channels (only if data exists)
    // These will be populated if physical activity data is available
    channels_.emplace_back("mean_physical_activity");
    channels_.emplace_back("std_physical_activity");

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

void AnalysisModule::initialise_income_output_channels(
    [[maybe_unused]] RuntimeContext &context) const {
    if (!enable_income_analysis_) {
        return;
    }

    // Use the same channels as regular output for income-based analysis
    // This will be called from add_income_channels in DataSeries
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

    auto module =
        std::make_unique<AnalysisModule>(std::move(definition), std::move(classifier),
                                         config.settings().age_range(), config.run().comorbidities);

    // Set income analysis flag from config - forces recompilation
    module->set_income_analysis_enabled(config.enable_income_analysis());

    return module;
}

} // namespace hgps
