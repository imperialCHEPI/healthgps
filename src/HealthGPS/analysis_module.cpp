
#include "analysis_module.h"
#include "HealthGPS.Core/thread_util.h"
#include "converter.h"
#include "lms_model.h"
#include "weight_model.h"

#include <cmath>
#include <functional>
#include <future>

namespace hgps {

/// @brief DALYs result unit conversion constant.
inline constexpr double DALY_UNITS = 100'000.0;

AnalysisModule::AnalysisModule(AnalysisDefinition &&definition, WeightModel &&classifier,
                               const core::IntegerInterval age_range, unsigned int comorbidities)
    : definition_{std::move(definition)}, weight_classifier_{std::move(classifier)},
      residual_disability_weight_{create_age_gender_table<double>(age_range)},
      comorbidities_{comorbidities} {}

SimulationModuleType AnalysisModule::type() const noexcept {
    return SimulationModuleType::Analysis;
}

const std::string &AnalysisModule::name() const noexcept { return name_; }

void AnalysisModule::initialise_population(RuntimeContext &context) {
    const auto &age_range = context.age_range();
    auto expected_sum = create_age_gender_table<double>(age_range);
    auto expected_count = create_age_gender_table<int>(age_range);
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};
    std::for_each(core::execution_policy, pop.cbegin(), pop.cend(), [&](const auto &entity) {
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

void AnalysisModule::update_population(RuntimeContext &context) { publish_result_message(context); }

double
AnalysisModule::calculate_residual_disability_weight(const int &age, const core::Gender gender,
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
    auto handle = core::run_async(&hgps::AnalysisModule::calculate_historical_statistics, this,
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
        core::run_async(&hgps::AnalysisModule::calculate_dalys, this,
                        std::ref(context.population()), age_upper_bound, analysis_time);

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

// NOLINTBEGIN(readability-function-cognitive-complexity)
void hgps::AnalysisModule::calculate_population_statistics(RuntimeContext &context,
                                                           DataSeries &series) const {
    using namespace core;

    auto min_age = context.age_range().lower();
    auto max_age = context.age_range().upper();
    if (series.size() > 0) {
        throw std::logic_error("This should be a new object!");
    }

    series.add_channels(channels_);

    auto current_time = static_cast<unsigned int>(context.time_now());
    for (const auto &entity : context.population()) {
        auto age = entity.age;
        auto gender = entity.gender;

        if (!entity.is_active()) {
            if (!entity.is_alive() && entity.time_of_death() == current_time) {
                series(gender, "deaths").at(age)++;
                auto expcted_life = definition_.life_expectancy().at(context.time_now(), gender);
                series(gender, "yll").at(age) += std::max(expcted_life - age, 0.0f) * DALY_UNITS;
            }

            if (entity.has_emigrated() && entity.time_of_migration() == current_time) {
                series(gender, "migrations").at(age)++;
            }

            continue;
        }

        series(gender, "count").at(age)++;
        for (const auto &factor : context.mapping().entries()) {
            series(gender, factor.key().to_string()).at(age) +=
                entity.get_risk_factor_value(factor.key());
        }

        for (const auto &item : entity.diseases) {
            if (item.second.status == DiseaseStatus::active) {
                series(gender, item.first.to_string()).at(age)++;
            }
        }

        auto dw = calculate_disability_weight(entity);
        series(gender, "disability_weight").at(age) += dw;
        series(gender, "yld").at(age) += (dw * DALY_UNITS);

        classify_weight(series, entity);
    }

    // Calculate DALY
    for (auto idx = min_age; idx <= max_age; idx++) {
        series(Gender::male, "daly").at(idx) =
            series(Gender::male, "yll").at(idx) + series(Gender::male, "yld").at(idx);
        series(Gender::female, "daly").at(idx) =
            series(Gender::female, "yll").at(idx) + series(Gender::female, "yld").at(idx);
    }

    // Calculate in-place averages
    for (auto index = min_age; index <= max_age; index++) {
        for (const auto &chan : series.channels()) {
            if (chan == "count") {
                continue;
            }

            auto real_count = series(Gender::male, "count").at(index);
            if (real_count > 0.0) {
                series(Gender::male, chan).at(index) =
                    series(Gender::male, chan).at(index) / real_count;
            } else {
                series(Gender::male, chan).at(index) = 0.0;
            }

            real_count = series(Gender::female, "count").at(index);
            if (real_count > 0.0) {
                series(Gender::female, chan).at(index) =
                    series(Gender::female, chan).at(index) / real_count;
            } else {
                series(Gender::female, chan).at(index) = 0.0;
            }
        }
    }
}
// NOLINTEND(readability-function-cognitive-complexity)

void AnalysisModule::classify_weight(hgps::DataSeries &series, const hgps::Person &entity) const {
    auto weight_class = weight_classifier_.classify_weight(entity);
    switch (weight_class) {
    case hgps::WeightCategory::normal:
        series(entity.gender, "normal_weight").at(entity.age)++;
        break;
    case hgps::WeightCategory::overweight:
        series(entity.gender, "over_weight").at(entity.age)++;
        series(entity.gender, "above_weight").at(entity.age)++;
        break;
    case hgps::WeightCategory::obese:
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
    for (const auto &factor : context.mapping().entries()) {
        channels_.emplace_back(factor.key().to_string());
    }

    for (const auto &disease : context.diseases()) {
        channels_.emplace_back(disease.code.to_string());
    }

    channels_.emplace_back("disability_weight");
    channels_.emplace_back("deaths");
    channels_.emplace_back("migrations");
    channels_.emplace_back("normal_weight");
    channels_.emplace_back("over_weight");
    channels_.emplace_back("obese_weight");
    channels_.emplace_back("above_weight");
    channels_.emplace_back("yll");
    channels_.emplace_back("yld");
    channels_.emplace_back("daly");
}

std::unique_ptr<AnalysisModule> build_analysis_module(Repository &repository,
                                                      const ModelInput &config) {
    auto analysis_entity = repository.manager().get_disease_analysis(config.settings().country());
    auto &lms_definition = repository.get_lms_definition();
    if (analysis_entity.empty() || lms_definition.empty()) {
        throw std::logic_error(
            "Failed to create analysis module, invalid disease analysis definition.");
    }

    auto definition = detail::StoreConverter::to_analysis_definition(analysis_entity);
    auto classifier = WeightModel{LmsModel{lms_definition}};

    return std::make_unique<AnalysisModule>(std::move(definition), std::move(classifier),
                                            config.settings().age_range(),
                                            config.run().comorbidities);
}

} // namespace hgps
