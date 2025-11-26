
#include "HealthGPS.Core/string_util.h"
#include "HealthGPS.Core/thread_util.h"

#include "analysis_module.h"
#include "converter.h"
#include "lms_model.h"
#include "weight_model.h"

#include <atomic>
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
    std::cout << "\nDEBUG: AnalysisModule::initialise_population - processing " << pop.size()
              << " people for residual weights";
    std::atomic_size_t processed{0};
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

        auto count = ++processed;
        if (count % 50000 == 0) {
            std::cout << "\nDEBUG: AnalysisModule residual pass processed " << count << " people";
        }
    });
    std::cout << "\nDEBUG: AnalysisModule residual pass completed";

    for (int age = age_range.lower(); age <= age_range.upper(); age++) {
        residual_disability_weight_(age, core::Gender::male) = calculate_residual_disability_weight(
            age, core::Gender::male, expected_sum, expected_count);

        residual_disability_weight_(age, core::Gender::female) =
            calculate_residual_disability_weight(age, core::Gender::female, expected_sum,
                                                 expected_count);
    }
    std::cout << "\nDEBUG: AnalysisModule residual disability weights calculated";

    initialise_output_channels(context);
    std::cout << "\nDEBUG: AnalysisModule output channels initialised";
    std::cout << "\nDEBUG: AnalysisModule about to publish initial results";
    publish_result_message(context);
    std::cout << "\nDEBUG: AnalysisModule initialisation completed";
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

    std::cout << "\nDEBUG: AnalysisModule publish_result_message - kicking off historical stats";
    auto handle = core::run_async(&AnalysisModule::calculate_historical_statistics, this,
                                  std::ref(context), std::ref(result));

    std::cout << "\nDEBUG: AnalysisModule publish_result_message - calculating population stats";
    calculate_population_statistics(context, result.series);
    std::cout << "\nDEBUG: AnalysisModule publish_result_message - waiting for historical stats";
    handle.get();
    std::cout << "\nDEBUG: AnalysisModule publish_result_message - historical stats done";

    context.publish(std::make_unique<ResultEventMessage>(
        context.identifier(), context.current_run(), context.time_now(), result));
    std::cout << "\nDEBUG: AnalysisModule publish_result_message - result sent";
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
void AnalysisModule::calculate_historical_statistics(RuntimeContext &context,
                                                     ModelResult &result) const {
    std::cout << "\nDEBUG: AnalysisModule calculate_historical_statistics - start";
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

    std::cout << "\nDEBUG: AnalysisModule calculate_historical_statistics - launching DALY task";
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

    std::cout << "\nDEBUG: AnalysisModule calculate_historical_statistics - waiting for DALY task";
    result.indicators = daly_handle.get();
    std::cout << "\nDEBUG: AnalysisModule calculate_historical_statistics - completed";

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
    std::cout << "\nDEBUG: AnalysisModule calculate_dalys - start";
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
    auto result = DALYsIndicator{.years_of_life_lost = yll,
                          .years_lived_with_disability = yld,
                          .disability_adjusted_life_years = yll + yld};
    std::cout << "\nDEBUG: AnalysisModule calculate_dalys - completed";
    return result;
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
    std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - start";
    std::cout << "\nDEBUG: Checking series size...";
    if (series.size() > 0) {
        throw std::logic_error("This should be a new object!");
    }
    std::cout << "\nDEBUG: Series size check passed, series.size()=" << series.size();
    std::cout << "\nDEBUG: About to add " << channels_.size() << " channels";
    std::cout.flush();
    series.add_channels(channels_);
    std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - channels ready";
    auto current_time = static_cast<unsigned int>(context.time_now());
    std::size_t processed = 0;
    std::size_t total_pop = context.population().size();
    std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - processing "
              << total_pop << " people";
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
        // Note: mean_age, mean_age2, mean_age3 will be set to age values AFTER division loop
        // Don't set them here to avoid being divided

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
            // Special handling for income_category - it's stored as person.income (enum), not in
            // risk_factors
            if (factor.key().to_string() == "income_category") {
                if (person.income != core::Income::unknown) {
                    double income_value = person.income_to_value();
                    series(gender, "mean_income_category").at(age) += income_value;
                }
                continue;
            }

            double factor_value = person.get_risk_factor_value(factor.key());
            series(gender, "mean_" + factor.key().to_string()).at(age) += factor_value;
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

        processed++;
        if (processed == 1 || processed == 10 || processed == 100 || processed % 50000 == 0) {
            std::cout
                << "\nDEBUG: AnalysisModule calculate_population_statistics(series) processed "
                << processed << " people";
        }
    }
    std::cout
        << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - completed, processed "
        << processed << " people";

    // For each age group in the analysis...
    const auto age_range = context.age_range();
    std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - computing "
                 "averages for age range ["
              << age_range.lower() << ", " << age_range.upper() << "]";
    std::cout.flush();

    // Create a set of available channels (lowercase) for safe access
    std::unordered_set<std::string> available_channels;
    for (const auto &channel : series.channels()) {
        available_channels.insert(core::to_lower(channel));
    }

    // Helper lambda to divide channel (only if channel exists)
    auto safe_divide_channel = [&](core::Gender gender, const std::string &channel_name, int age,
                                   double count) {
        std::string channel_key = core::to_lower(channel_name);
        // Only access if channel exists
        if (count > 0 && available_channels.find(channel_key) != available_channels.end()) {
            series(gender, channel_name).at(age) /= count;
        }
    };

    for (int age = age_range.lower(); age <= age_range.upper(); age++) {
        if (age == age_range.lower() || age % 10 == 0 || age == age_range.upper()) {
            std::cout << "\nDEBUG: Computing averages for age " << age;
            std::cout.flush();
        }

        double count_F = series(core::Gender::female, "count").at(age);
        double count_M = series(core::Gender::male, "count").at(age);
        double deaths_F = series(core::Gender::female, "deaths").at(age);
        double deaths_M = series(core::Gender::male, "deaths").at(age);

        // Calculate in-place factor averages.
        for (const auto &factor : context.mapping().entries()) {
            std::string column = "mean_" + factor.key().to_string();
            safe_divide_channel(core::Gender::female, column, age, count_F);
            safe_divide_channel(core::Gender::male, column, age, count_M);
        }

        // MAHIMA: Calculate in-place demographic averages (only if data exists)
        // These channels may not exist if data wasn't populated - use safe access
        safe_divide_channel(core::Gender::female, "mean_region", age, count_F);
        safe_divide_channel(core::Gender::male, "mean_region", age, count_M);
        safe_divide_channel(core::Gender::female, "mean_ethnicity", age, count_F);
        safe_divide_channel(core::Gender::male, "mean_ethnicity", age, count_M);
        safe_divide_channel(core::Gender::female, "mean_sector", age, count_F);
        safe_divide_channel(core::Gender::male, "mean_sector", age, count_M);
        safe_divide_channel(core::Gender::female, "mean_income_category", age, count_F);
        safe_divide_channel(core::Gender::male, "mean_income_category", age, count_M);
        safe_divide_channel(core::Gender::female, "mean_income_continuous", age, count_F);
        safe_divide_channel(core::Gender::male, "mean_income_continuous", age, count_M);
        safe_divide_channel(core::Gender::female, "mean_physical_activity", age, count_F);
        safe_divide_channel(core::Gender::male, "mean_physical_activity", age, count_M);

        // Calculate in-place disease prevalence and incidence rates.
        for (const auto &disease : context.diseases()) {
            std::string column_prevalence = "prevalence_" + disease.code.to_string();
            std::string column_incidence = "incidence_" + disease.code.to_string();
            safe_divide_channel(core::Gender::female, column_prevalence, age, count_F);
            safe_divide_channel(core::Gender::female, column_incidence, age, count_F);
            safe_divide_channel(core::Gender::male, column_prevalence, age, count_M);
            safe_divide_channel(core::Gender::male, column_incidence, age, count_M);
        }

        // Calculate in-place YLL/YLD/DALY averages.
        for (const auto &column : {"mean_yll", "mean_yld", "mean_daly"}) {
            safe_divide_channel(core::Gender::female, column, age, count_F + deaths_F);
            safe_divide_channel(core::Gender::male, column, age, count_M + deaths_M);
        }

        // NEW: Set mean_age, mean_age2, mean_age3 to age values (not averages)
        // These should be the age itself (0, 1, 2, ...), not the mean of ages
        series(core::Gender::female, "mean_age").at(age) = static_cast<double>(age);
        series(core::Gender::male, "mean_age").at(age) = static_cast<double>(age);
        series(core::Gender::female, "mean_age2").at(age) = static_cast<double>(age * age);
        series(core::Gender::male, "mean_age2").at(age) = static_cast<double>(age * age);
        series(core::Gender::female, "mean_age3").at(age) = static_cast<double>(age * age * age);
        series(core::Gender::male, "mean_age3").at(age) = static_cast<double>(age * age * age);
    }
    std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - averages "
                 "computation completed";
    std::cout.flush();

    // Calculate standard deviation
    calculate_standard_deviation(context, series);

    // Add income-based analysis if enabled
    std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - checking "
                 "income analysis (enabled="
              << (enable_income_analysis_ ? "true" : "false") << ")";
    std::cout.flush();
    if (enable_income_analysis_) {
        std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - starting "
                     "income-based analysis";
        std::cout.flush();
        calculate_income_based_population_statistics(context, series);
        std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - "
                     "income-based analysis completed";
        std::cout.flush();
    }
    std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - ALL COMPLETED";
    std::cout.flush();
}
// NOLINTEND(readability-function-cognitive-complexity)

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void AnalysisModule::calculate_income_based_population_statistics(RuntimeContext &context,
                                                                  DataSeries &series) const {
    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - start";
    std::cout.flush();

    if (!enable_income_analysis_) {
        std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - "
                     "income analysis disabled, returning";
        std::cout.flush();
        return;
    }

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - getting "
                 "available income categories";
    std::cout.flush();
    auto available_income_categories = get_available_income_categories(context);
    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - found "
              << available_income_categories.size() << " income categories";
    std::cout.flush();

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

    // Check population to see which attributes are actually assigned
    bool has_region = false;
    bool has_ethnicity = false;
    bool has_sector = false;
    bool has_income_category = false;
    bool has_income_continuous = false;
    bool has_physical_activity = false;

    // Sample a subset of the population to check for attribute availability
    const auto &pop = context.population();
    size_t sample_size = std::min(pop.size(), static_cast<size_t>(1000));
    for (size_t i = 0; i < sample_size; i++) {
        const auto &person = pop[i];
        if (!person.is_active()) {
            continue;
        }

        if (!person.region.empty() && person.region != "unknown") {
            has_region = true;
        }
        if (!person.ethnicity.empty() && person.ethnicity != "unknown") {
            has_ethnicity = true;
        }
        if (person.sector != core::Sector::unknown) {
            has_sector = true;
        }
        if (person.income != core::Income::unknown) {
            has_income_category = true;
        }
        if (person.risk_factors.find("income_continuous"_id) != person.risk_factors.end()) {
            has_income_continuous = true;
        }
        if (person.risk_factors.find("PhysicalActivity"_id) != person.risk_factors.end()) {
            has_physical_activity = true;
        }

        // Early exit if all attributes found
        if (has_region && has_ethnicity && has_sector && has_income_category &&
            has_income_continuous && has_physical_activity) {
            break;
        }
    }

    // MAHIMA: Add demographic channels for income analysis (only if data exists)
    if (has_sector) {
    income_channels.emplace_back("mean_sector");
    income_channels.emplace_back("std_sector");
    }
    if (has_region) {
    income_channels.emplace_back("mean_region");
    income_channels.emplace_back("std_region");
    }
    if (has_ethnicity) {
    income_channels.emplace_back("mean_ethnicity");
    income_channels.emplace_back("std_ethnicity");
    }

    // MAHIMA: Add enhanced income channels for income analysis (only if data exists)
    if (has_income_category) {
    income_channels.emplace_back("mean_income_category");
    income_channels.emplace_back("std_income_category");
    }
    if (has_income_continuous) {
    income_channels.emplace_back("mean_income_continuous");
    income_channels.emplace_back("std_income_continuous");
    }

    // MAHIMA: Add physical activity channels for income analysis (only if data exists)
    if (has_physical_activity) {
    income_channels.emplace_back("mean_physical_activity");
    income_channels.emplace_back("std_physical_activity");
    }

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

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - "
                 "collecting income categories";
    std::cout.flush();

    // Create income channels for the actual income categories found in the data
    std::vector<core::Income> actual_income_categories;
    for (const auto &person : context.population()) {
        if (std::ranges::find(actual_income_categories, person.income) ==
            actual_income_categories.end()) {
            actual_income_categories.emplace_back(person.income);
        }
    }

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - found "
              << actual_income_categories.size() << " actual income categories";
    std::cout.flush();

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - adding "
                 "income channels";
    std::cout.flush();
    // Create income channels for the actual categories found
    series.add_income_channels_for_categories(income_channels, actual_income_categories);
    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - income "
                 "channels added";
    std::cout.flush();

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - "
                 "initializing std dev channels";
    std::cout.flush();

    // Initialize standard deviation channels with zeros (with safe access)
    const auto age_range = context.age_range();

    // Track which channels were actually added (lowercase keys) - used by all lambdas
    std::unordered_set<std::string> added_income_channels;
    for (const auto &channel : income_channels) {
        added_income_channels.insert(core::to_lower(channel));
    }

    auto safe_init_channel = [&series,
                              &added_income_channels](core::Gender gender, core::Income income,
                                                      const std::string &channel, int age) {
        std::string channel_key = core::to_lower(channel);
        // Only initialize if channel was added
        if (added_income_channels.find(channel_key) != added_income_channels.end()) {
            series.at(gender, income, channel_key).at(age) = 0.0;
        }
    };

    for (const auto &income : actual_income_categories) {
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            for (const auto &factor : context.mapping().entries()) {
                safe_init_channel(core::Gender::female, income, "std_" + factor.key().to_string(),
                                  age);
                safe_init_channel(core::Gender::male, income, "std_" + factor.key().to_string(),
                                  age);
            }
            for (const auto &column : {"std_yll", "std_yld", "std_daly"}) {
                safe_init_channel(core::Gender::female, income, column, age);
                safe_init_channel(core::Gender::male, income, column, age);
            }
            for (const auto &column : {"std_income", "std_sector"}) {
                safe_init_channel(core::Gender::female, income, column, age);
                safe_init_channel(core::Gender::male, income, column, age);
            }

            // NEW: Initialize age standard deviation channels with zeros (always available)
            for (const auto &column : {"std_age2", "std_age3"}) {
                safe_init_channel(core::Gender::female, income, column, age);
                safe_init_channel(core::Gender::male, income, column, age);
            }

            // NEW: Initialize enhanced demographic standard deviation channels with zeros (only if
            // data exists)
            for (const auto &column : {"std_region", "std_ethnicity", "std_income_category",
                                       "std_income_continuous", "std_physical_activity"}) {
                safe_init_channel(core::Gender::female, income, column, age);
                safe_init_channel(core::Gender::male, income, column, age);
            }
        }
    }

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - std dev "
                 "channels initialized";
    std::cout.flush();

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - starting "
                 "main processing loop";
    std::cout.flush();

    auto current_time = static_cast<unsigned int>(context.time_now());
    std::size_t processed_income = 0;

    auto safe_add_to_channel =
        [&series, &added_income_channels](core::Gender gender, core::Income income,
                                          const std::string &channel, int age, double value) {
            std::string channel_key = core::to_lower(channel);
            // Only access if channel was added
            if (added_income_channels.find(channel_key) != added_income_channels.end()) {
                series.at(gender, income, channel_key).at(age) += value;
            }
        };

    auto safe_increment_channel = [&series,
                                   &added_income_channels](core::Gender gender, core::Income income,
                                                           const std::string &channel, int age) {
        std::string channel_key = core::to_lower(channel);
        // Only access if channel was added
        if (added_income_channels.find(channel_key) != added_income_channels.end()) {
            series.at(gender, income, channel_key).at(age)++;
        }
    };

    for (const auto &person : context.population()) {

        auto age = person.age;
        auto gender = person.gender;
        auto income = person.income;

        if (!person.is_active()) {
            if (!person.is_alive() && person.time_of_death() == current_time) {
                safe_increment_channel(gender, income, "deaths", age);
                float expcted_life = definition_.life_expectancy().at(context.time_now(), gender);
                    double yll = std::max(expcted_life - age, 0.0f) * DALY_UNITS;
                safe_add_to_channel(gender, income, "mean_yll", age, yll);
                safe_add_to_channel(gender, income, "mean_daly", age, yll);
            }

            if (person.has_emigrated() && person.time_of_migration() == current_time) {
                safe_increment_channel(gender, income, "emigrations", age);
            }

            continue;
        }

        safe_increment_channel(gender, income, "count", age);

        for (const auto &factor : context.mapping().entries()) {
            // Special handling for income_category - it's stored as person.income (enum), not in
            // risk_factors
            if (factor.key().to_string() == "income_category") {
                if (person.income != core::Income::unknown) {
                    double income_value = person.income_to_value();
                    safe_add_to_channel(gender, income, "mean_income_category", age, income_value);
                }
                continue;
            }

            auto channel_name = "mean_" + factor.key().to_string();
            double value = person.get_risk_factor_value(factor.key());
            safe_add_to_channel(gender, income, channel_name, age, value);
        }

        // Collect demographic data (excluding age and gender)
        safe_add_to_channel(gender, income, "mean_income", age,
                            static_cast<double>(static_cast<int>(person.income)));
        safe_add_to_channel(gender, income, "mean_sector", age,
                            static_cast<double>(static_cast<int>(person.sector)));

        // NEW: Collect enhanced demographic data by income (only if available)
            // Region data - check if person has region assigned
            if (!person.region.empty() && person.region != "unknown") {
            safe_add_to_channel(gender, income, "mean_region", age, person.region_to_value());
            }

            // Ethnicity data - check if person has ethnicity assigned
            if (!person.ethnicity.empty() && person.ethnicity != "unknown") {
            safe_add_to_channel(gender, income, "mean_ethnicity", age, person.ethnicity_to_value());
            }

            // Income category - check if person has income assigned
            if (person.income != core::Income::unknown) {
            safe_add_to_channel(gender, income, "mean_income_category", age,
                                person.income_to_value());
            }

            // Continuous income - check if available in risk factors
            auto it = person.risk_factors.find("income_continuous"_id);
            if (it != person.risk_factors.end()) {
            safe_add_to_channel(gender, income, "mean_income_continuous", age, it->second);
            }

            // Physical activity - check if available in risk factors
            auto pa_it = person.risk_factors.find("PhysicalActivity"_id);
            if (pa_it != person.risk_factors.end()) {
            safe_add_to_channel(gender, income, "mean_physical_activity", age, pa_it->second);
        }

        for (const auto &[disease_name, disease_state] : person.diseases) {
            if (disease_state.status == DiseaseStatus::active) {
                auto prevalence_channel = "prevalence_" + disease_name.to_string();
                safe_increment_channel(gender, income, prevalence_channel, age);
                if (disease_state.start_time == context.time_now()) {
                    auto incidence_channel = "incidence_" + disease_name.to_string();
                    safe_increment_channel(gender, income, incidence_channel, age);
                }
            }
        }

        double dw = calculate_disability_weight(person);
        double yld = dw * DALY_UNITS;
        safe_add_to_channel(gender, income, "mean_yld", age, yld);
        safe_add_to_channel(gender, income, "mean_daly", age, yld);

        // Classify weight for income-based analysis
        auto weight_class = weight_classifier_.classify_weight(person);
        switch (weight_class) {
        case WeightCategory::normal:
            safe_increment_channel(gender, income, "normal_weight", age);
            break;
        case WeightCategory::overweight:
            safe_increment_channel(gender, income, "over_weight", age);
            safe_increment_channel(gender, income, "above_weight", age);
            break;
        case WeightCategory::obese:
            safe_increment_channel(gender, income, "obese_weight", age);
            safe_increment_channel(gender, income, "above_weight", age);
            break;
        default:
            // Unknown weight classification - skip it
            break;
        }

        processed_income++;
        if (processed_income == 1 || processed_income == 10 || processed_income == 100 ||
            processed_income % 50000 == 0) {
            std::cout
                << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics processed "
                << processed_income << " people";
            std::cout.flush();
        }
    }

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - main loop "
                 "completed, processed "
              << processed_income << " people";
    std::cout.flush();

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - "
                 "calculating averages";
    std::cout.flush();

    // Calculate averages for each income category (with safe channel access)
    auto safe_divide_channel_income = [&series, &added_income_channels](
                                          core::Gender gender, core::Income income,
                                          const std::string &channel, int age, double count) {
        std::string channel_key = core::to_lower(channel);
        // Only access if channel was added
        if (count > 0 && added_income_channels.find(channel_key) != added_income_channels.end()) {
            series.at(gender, income, channel_key).at(age) /= count;
        }
    };

    for (const auto &income : available_income_categories) {
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            double count_F = series.at(core::Gender::female, income, "count").at(age);
            double count_M = series.at(core::Gender::male, income, "count").at(age);
            double deaths_F = series.at(core::Gender::female, income, "deaths").at(age);
            double deaths_M = series.at(core::Gender::male, income, "deaths").at(age);

            // Calculate in-place factor averages for this income category
            for (const auto &factor : context.mapping().entries()) {
                std::string column = "mean_" + factor.key().to_string();
                safe_divide_channel_income(core::Gender::female, income, column, age, count_F);
                safe_divide_channel_income(core::Gender::male, income, column, age, count_M);
            }

            // Calculate in-place demographic averages for this income category (excluding age and
            // gender)
            safe_divide_channel_income(core::Gender::female, income, "mean_income", age, count_F);
            safe_divide_channel_income(core::Gender::female, income, "mean_sector", age, count_F);
            safe_divide_channel_income(core::Gender::male, income, "mean_income", age, count_M);
            safe_divide_channel_income(core::Gender::male, income, "mean_sector", age, count_M);

            // MAHIMA: Calculate in-place enhanced demographic averages for this income category
            // (only if data exists)
            safe_divide_channel_income(core::Gender::female, income, "mean_region", age, count_F);
            safe_divide_channel_income(core::Gender::female, income, "mean_ethnicity", age,
                                       count_F);
            safe_divide_channel_income(core::Gender::female, income, "mean_income_category", age,
                                       count_F);
            safe_divide_channel_income(core::Gender::female, income, "mean_income_continuous", age,
                                       count_F);
            safe_divide_channel_income(core::Gender::female, income, "mean_physical_activity", age,
                                       count_F);
            safe_divide_channel_income(core::Gender::male, income, "mean_region", age, count_M);
            safe_divide_channel_income(core::Gender::male, income, "mean_ethnicity", age, count_M);
            safe_divide_channel_income(core::Gender::male, income, "mean_income_category", age,
                                       count_M);
            safe_divide_channel_income(core::Gender::male, income, "mean_income_continuous", age,
                                       count_M);
            safe_divide_channel_income(core::Gender::male, income, "mean_physical_activity", age,
                                       count_M);

            // Calculate in-place disease prevalence and incidence rates for this income category
            for (const auto &disease : context.diseases()) {
                std::string column_prevalence = "prevalence_" + disease.code.to_string();
                safe_divide_channel_income(core::Gender::female, income, column_prevalence, age,
                                           count_F);
                safe_divide_channel_income(core::Gender::male, income, column_prevalence, age,
                                           count_M);

                std::string column_incidence = "incidence_" + disease.code.to_string();
                safe_divide_channel_income(core::Gender::female, income, column_incidence, age,
                                           count_F);
                safe_divide_channel_income(core::Gender::male, income, column_incidence, age,
                                           count_M);
            }

            // Calculate in-place YLL/YLD/DALY averages for this income category
            for (const auto &column : {"mean_yll", "mean_yld", "mean_daly"}) {
                safe_divide_channel_income(core::Gender::female, income, column, age,
                                           count_F + deaths_F);
                safe_divide_channel_income(core::Gender::male, income, column, age,
                                           count_M + deaths_M);
            }
        }
    }

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - averages "
                 "computation completed";
    std::cout.flush();

    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - starting "
                 "standard deviation calculation";
    std::cout.flush();
    // Calculate standard deviation for income-based data
    calculate_income_based_standard_deviation(context, series);
    std::cout << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - standard "
                 "deviation calculation completed";
    std::cout.flush();

    std::cout
        << "\nDEBUG: AnalysisModule calculate_income_based_population_statistics - ALL COMPLETED";
    std::cout.flush();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void AnalysisModule::calculate_income_based_standard_deviation(RuntimeContext &context,
                                                               DataSeries &series) const {
    // Recreate the list of income channels that were added (same as in
    // calculate_income_based_population_statistics)
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
    income_channels.emplace_back("mean_income");
    income_channels.emplace_back("std_income");

    // Check population to see which attributes are actually assigned (same logic as calculate_income_based_population_statistics)
    bool has_region = false;
    bool has_ethnicity = false;
    bool has_sector = false;
    bool has_income_category = false;
    bool has_income_continuous = false;
    bool has_physical_activity = false;

    // Sample a subset of the population to check for attribute availability
    const auto &pop = context.population();
    size_t sample_size = std::min(pop.size(), static_cast<size_t>(1000));
    for (size_t i = 0; i < sample_size; i++) {
        const auto &person = pop[i];
        if (!person.is_active()) {
            continue;
        }

        if (!person.region.empty() && person.region != "unknown") {
            has_region = true;
        }
        if (!person.ethnicity.empty() && person.ethnicity != "unknown") {
            has_ethnicity = true;
        }
        if (person.sector != core::Sector::unknown) {
            has_sector = true;
        }
        if (person.income != core::Income::unknown) {
            has_income_category = true;
        }
        if (person.risk_factors.find("income_continuous"_id) != person.risk_factors.end()) {
            has_income_continuous = true;
        }
        if (person.risk_factors.find("PhysicalActivity"_id) != person.risk_factors.end()) {
            has_physical_activity = true;
        }

        // Early exit if all attributes found
        if (has_region && has_ethnicity && has_sector && has_income_category &&
            has_income_continuous && has_physical_activity) {
            break;
        }
    }

    // MAHIMA: Add demographic channels for income analysis (only if data exists)
    if (has_sector) {
        income_channels.emplace_back("mean_sector");
        income_channels.emplace_back("std_sector");
    }
    if (has_region) {
        income_channels.emplace_back("mean_region");
        income_channels.emplace_back("std_region");
    }
    if (has_ethnicity) {
        income_channels.emplace_back("mean_ethnicity");
        income_channels.emplace_back("std_ethnicity");
    }

    // MAHIMA: Add enhanced income channels for income analysis (only if data exists)
    if (has_income_category) {
        income_channels.emplace_back("mean_income_category");
        income_channels.emplace_back("std_income_category");
    }
    if (has_income_continuous) {
        income_channels.emplace_back("mean_income_continuous");
        income_channels.emplace_back("std_income_continuous");
    }

    // MAHIMA: Add physical activity channels for income analysis (only if data exists)
    if (has_physical_activity) {
        income_channels.emplace_back("mean_physical_activity");
        income_channels.emplace_back("std_physical_activity");
    }

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

    // Track which channels were actually added (lowercase keys)
    std::unordered_set<std::string> added_income_channels;
    for (const auto &channel : income_channels) {
        added_income_channels.insert(core::to_lower(channel));
    }

    // Accumulate squared deviations from mean for income-based data
    auto accumulate_squared_diffs_income =
        [&series, &added_income_channels](const std::string &chan, core::Gender sex,
                                                     core::Income income, int age, double value) {
            std::string mean_channel = core::to_lower("mean_" + chan);
            std::string std_channel = core::to_lower("std_" + chan);
            // Only accumulate if both mean and std channels were added
            if (added_income_channels.find(mean_channel) != added_income_channels.end() &&
                added_income_channels.find(std_channel) != added_income_channels.end()) {
                const double mean = series.at(sex, income, mean_channel).at(age);
        const double diff = value - mean;
                series.at(sex, income, std_channel).at(age) += diff * diff;
            }
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
            // Special handling for income_category - it's stored as person.income (enum), not in
            // risk_factors
            if (factor.key().to_string() == "income_category") {
                if (person.income != core::Income::unknown) {
                    double income_value = person.income_to_value();
                    accumulate_squared_diffs_income("income_category", sex, income, age,
                                                    income_value);
                }
                continue;
            }

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
    auto divide_by_count_sqrt_income =
        [&series, &added_income_channels](const std::string &chan, core::Gender sex,
                                                 core::Income income, int age, double count) {
            std::string std_channel = core::to_lower("std_" + chan);
            // Only access if channel was added
            if (added_income_channels.find(std_channel) != added_income_channels.end()) {
        if (count > 0) {
                    const double sum = series.at(sex, income, std_channel).at(age);
            const double std = std::sqrt(sum / count);
                    series.at(sex, income, std_channel).at(age) = std;
        } else {
                    series.at(sex, income, std_channel).at(age) = 0.0;
                }
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

    std::cout << "\nDEBUG: AnalysisModule calculate_population_statistics(series) - completed";
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void AnalysisModule::calculate_standard_deviation(RuntimeContext &context,
                                                  DataSeries &series) const {
    std::cout << "\nDEBUG: AnalysisModule calculate_standard_deviation - start";
    std::cout.flush();

    // Create a set of available channels (lowercase) for safe access
    std::unordered_set<std::string> available_channels;
    for (const auto &channel : series.channels()) {
        available_channels.insert(core::to_lower(channel));
    }

    // Accumulate squared deviations from mean (with safe channel access).
    auto accumulate_squared_diffs = [&series, &available_channels](const std::string &chan, core::Gender sex, int age,
                                              double value) {
        std::string mean_channel = core::to_lower("mean_" + chan);
        std::string std_channel = core::to_lower("std_" + chan);
        // Only accumulate if both mean and std channels exist
        if (available_channels.find(mean_channel) != available_channels.end() &&
            available_channels.find(std_channel) != available_channels.end()) {
        const double mean = series(sex, "mean_" + chan).at(age);
        const double diff = value - mean;
        series(sex, "std_" + chan).at(age) += diff * diff;
        }
    };

    std::size_t processed_std = 0;
    std::size_t total_pop_std = context.population().size();
    std::cout << "\nDEBUG: AnalysisModule calculate_standard_deviation - processing "
              << total_pop_std << " people";
    std::cout.flush();

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
            // Special handling for income_category - it's stored as person.income (enum), not in
            // risk_factors
            if (factor.key().to_string() == "income_category") {
                if (person.income != core::Income::unknown) {
                    double income_value = person.income_to_value();
                    accumulate_squared_diffs("income_category", sex, age, income_value);
                }
                continue;
            }

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

        processed_std++;
        if (processed_std == 1 || processed_std == 10 || processed_std == 100 ||
            processed_std % 50000 == 0) {
            std::cout << "\nDEBUG: AnalysisModule calculate_standard_deviation processed "
                      << processed_std << " people";
            std::cout.flush();
        }
    }
    std::cout << "\nDEBUG: AnalysisModule calculate_standard_deviation - accumulation completed, "
                 "processed "
              << processed_std << " people";
    std::cout.flush();

    // Calculate in-place standard deviation (only if channel exists)
    // Note: available_channels was already created above for accumulate_squared_diffs
    auto divide_by_count_sqrt = [&series, &available_channels](const std::string &chan, core::Gender sex, int age,
                                          double count) {
        std::string std_channel = core::to_lower("std_" + chan);
        // Only access if channel exists
        if (available_channels.find(std_channel) != available_channels.end()) {
        if (count > 0) {
            const double sum = series(sex, "std_" + chan).at(age);
            const double std = std::sqrt(sum / count);
            series(sex, "std_" + chan).at(age) = std;
        } else {
            series(sex, "std_" + chan).at(age) = 0.0;
            }
        }
    };

    // For each age group in the analysis...
    const auto age_range = context.age_range();
    std::cout << "\nDEBUG: AnalysisModule calculate_standard_deviation - computing std dev for age "
                 "range ["
              << age_range.lower() << ", " << age_range.upper() << "]";
    std::cout.flush();

    for (int age = age_range.lower(); age <= age_range.upper(); age++) {
        if (age == age_range.lower() || age % 10 == 0 || age == age_range.upper()) {
            std::cout << "\nDEBUG: Computing std dev for age " << age;
            std::cout.flush();
        }

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
    std::cout << "\nDEBUG: AnalysisModule calculate_standard_deviation - completed";
    std::cout.flush();
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

    // Track added channels (lowercase) to avoid duplicates
    std::unordered_set<std::string> added_channels;

    auto add_channel_if_new = [&](const std::string &channel_name) {
        std::string lower_channel = core::to_lower(channel_name);
        if (added_channels.find(lower_channel) == added_channels.end()) {
            channels_.emplace_back(channel_name);
            added_channels.insert(lower_channel);
        }
    };

    add_channel_if_new("count");
    add_channel_if_new("deaths");
    add_channel_if_new("emigrations");

    // NEW: Add age channels (always available)
    add_channel_if_new("mean_age");
    add_channel_if_new("std_age");
    add_channel_if_new("mean_age2");
    add_channel_if_new("std_age2");
    add_channel_if_new("mean_age3");
    add_channel_if_new("std_age3");

    // Check population to see which attributes are actually assigned
    bool has_region = false;
    bool has_ethnicity = false;
    bool has_sector = false;
    bool has_income_category = false;
    bool has_income_continuous = false;
    bool has_physical_activity = false;

    // Sample a subset of the population to check for attribute availability
    const auto &pop = context.population();
    size_t sample_size = std::min(pop.size(), static_cast<size_t>(1000));
    for (size_t i = 0; i < sample_size; i++) {
        const auto &person = pop[i];
        if (!person.is_active()) {
            continue;
        }

        if (!person.region.empty() && person.region != "unknown") {
            has_region = true;
        }
        if (!person.ethnicity.empty() && person.ethnicity != "unknown") {
            has_ethnicity = true;
        }
        if (person.sector != core::Sector::unknown) {
            has_sector = true;
        }
        if (person.income != core::Income::unknown) {
            has_income_category = true;
        }
        if (person.risk_factors.find("income_continuous"_id) != person.risk_factors.end()) {
            has_income_continuous = true;
        }
        if (person.risk_factors.find("PhysicalActivity"_id) != person.risk_factors.end()) {
            has_physical_activity = true;
        }

        // Early exit if all attributes found
        if (has_region && has_ethnicity && has_sector && has_income_category &&
            has_income_continuous && has_physical_activity) {
            break;
        }
    }

    // NEW: Add demographic channels (only if data exists)
    if (has_region) {
        add_channel_if_new("mean_region");
        add_channel_if_new("std_region");
    }
    if (has_ethnicity) {
        add_channel_if_new("mean_ethnicity");
        add_channel_if_new("std_ethnicity");
    }
    if (has_sector) {
        add_channel_if_new("mean_sector");
        add_channel_if_new("std_sector");
    }

    // NEW: Add income channels (only if data exists)
    if (has_income_category) {
        add_channel_if_new("mean_income_category");
        add_channel_if_new("std_income_category");
    }
    if (has_income_continuous) {
        add_channel_if_new("mean_income_continuous");
        add_channel_if_new("std_income_continuous");
    }

    // NEW: Add physical activity channels (only if data exists)
    if (has_physical_activity) {
        add_channel_if_new("mean_physical_activity");
        add_channel_if_new("std_physical_activity");
    }

    // Add channels for risk factors (skip if already added explicitly above)
    for (const auto &factor : context.mapping().entries()) {
        add_channel_if_new("mean_" + factor.key().to_string());
        add_channel_if_new("std_" + factor.key().to_string());
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
