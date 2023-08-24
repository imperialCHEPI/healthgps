#include "converter.h"

#include "HealthGPS.Core/string_util.h"
#include "default_cancer_model.h"
#include <fmt/format.h>

namespace hgps::detail {
core::Gender StoreConverter::to_gender(const std::string name) {
    if (core::case_insensitive::equals(name, "male")) {
        return core::Gender::male;
    }

    if (core::case_insensitive::equals(name, "female")) {
        return core::Gender::female;
    }

    return core::Gender::unknown;
}

DiseaseTable StoreConverter::to_disease_table(const core::DiseaseEntity &entity) {
    auto measures = entity.measures;
    auto data = std::map<int, std::map<core::Gender, DiseaseMeasure>>();
    for (const auto &v : entity.items) {
        data[v.with_age][v.gender] = DiseaseMeasure(v.measures);
    }

    return {entity.info, std::move(measures), std::move(data)};
}

FloatAgeGenderTable StoreConverter::to_relative_risk_table(const core::RelativeRiskEntity &entity) {
    auto num_rows = entity.rows.size();
    auto num_cols = entity.columns.size() - 1;
    auto cols = std::vector<core::Gender>();
    for (size_t i = 1; i <= num_cols; i++) {
        auto gender = to_gender(entity.columns[i]);
        if (gender == core::Gender::unknown) {
            throw std::out_of_range(
                fmt::format("Invalid column gender type: {}", entity.columns[i]));
        }

        cols.emplace_back(gender);
    }

    auto rows = std::vector<int>(num_rows);
    auto data = core::FloatArray2D(num_rows, num_cols);
    for (size_t i = 0; i < num_rows; i++) {
        const auto &row = entity.rows[i];
        rows[i] = static_cast<int>(row[0]);
        for (size_t j = 1; j < row.size(); j++) {
            data(i, j - 1) = row[j];
        }
    }

    return {MonotonicVector(rows), cols, std::move(data)};
}

RelativeRiskLookup StoreConverter::to_relative_risk_lookup(const core::RelativeRiskEntity &entity) {
    auto num_rows = entity.rows.size();
    auto num_cols = entity.columns.size() - 1;
    auto cols = std::vector<float>();
    for (size_t i = 1; i <= num_cols; i++) {
        cols.emplace_back(std::stof(entity.columns[i]));
    }

    auto rows = std::vector<int>(num_rows);
    auto data = core::FloatArray2D(num_rows, num_cols);
    for (size_t i = 0; i < num_rows; i++) {
        auto row = entity.rows[i];
        rows[i] = static_cast<int>(row[0]);
        for (size_t j = 1; j < row.size(); j++) {
            data(i, j - 1) = row[j];
        }
    }

    return {MonotonicVector(rows), MonotonicVector(cols), std::move(data)};
}

RelativeRisk create_relative_risk(const RelativeRiskInfo &info) {
    RelativeRisk result;
    for (const auto &item : info.inputs.diseases()) {
        auto table = info.manager.get_relative_risk_to_disease(info.disease, item);
        if (!table.empty() && !table.is_default_value) {
            result.diseases.emplace(item.code, StoreConverter::to_relative_risk_table(table));
        }
    }

    for (auto &factor : info.risk_factors) {
        auto table_male = info.manager.get_relative_risk_to_risk_factor(
            info.disease, core::Gender::male, factor.key());
        auto table_feme = info.manager.get_relative_risk_to_risk_factor(
            info.disease, core::Gender::female, factor.key());

        if (!table_male.empty()) {
            result.risk_factors[factor.key()].emplace(
                core::Gender::male, StoreConverter::to_relative_risk_lookup(table_male));
        }

        if (!table_feme.empty()) {
            result.risk_factors[factor.key()].emplace(
                core::Gender::female, StoreConverter::to_relative_risk_lookup(table_feme));
        }
    }

    return result;
}

AnalysisDefinition
StoreConverter::to_analysis_definition(const core::DiseaseAnalysisEntity &entity) {
    auto cols = std::vector<core::Gender>{core::Gender::male, core::Gender::female};
    auto lex_rows = std::vector<int>(entity.life_expectancy.size());
    auto lex_data = core::FloatArray2D(lex_rows.size(), cols.size());
    for (size_t index = 0; index < entity.life_expectancy.size(); index++) {
        const auto &item = entity.life_expectancy.at(index);
        lex_rows[index] = item.at_time;
        lex_data(index, 0) = item.male;
        lex_data(index, 1) = item.female;
    }

    auto life_expectancy =
        GenderTable<int, float>(MonotonicVector(lex_rows), cols, std::move(lex_data));

    auto min_time = entity.cost_of_diseases.begin()->first;
    auto max_time = entity.cost_of_diseases.rbegin()->first;
    auto cost_of_disease =
        create_age_gender_table<double>(core::IntegerInterval(min_time, max_time));
    for (const auto &item : entity.cost_of_diseases) {
        cost_of_disease(item.first, core::Gender::male) = item.second.at(core::Gender::male);
        cost_of_disease(item.first, core::Gender::female) = item.second.at(core::Gender::female);
    }

    auto weights = std::map<core::Identifier, float>{};
    for (const auto &item : entity.disability_weights) {
        weights.emplace(core::Identifier{item.first}, item.second);
    }

    return {std::move(life_expectancy), std::move(cost_of_disease), std::move(weights)};
}

LifeTable StoreConverter::to_life_table(const std::vector<core::BirthItem> &births,
                                        const std::vector<core::MortalityItem> &deaths) {
    auto table_births = std::map<int, Birth>{};
    for (const auto &item : births) {
        table_births.emplace(item.at_time, Birth{item.number, item.sex_ratio});
    }

    auto table_deaths = std::map<int, std::map<int, Mortality>>{};
    for (const auto &item : deaths) {
        table_deaths[item.at_time].emplace(item.with_age, Mortality(item.males, item.females));
    }

    return {std::move(table_births), std::move(table_deaths)};
}

DiseaseParameter StoreConverter::to_disease_parameter(const core::CancerParameterEntity &entity) {
    auto distribution = ParameterLookup{};
    for (const auto &item : entity.prevalence_distribution) {
        distribution.emplace(item.value, DoubleGenderValue(item.male, item.female));
    }

    auto survival = ParameterLookup{};
    for (const auto &item : entity.survival_rate) {
        survival.emplace(item.value, DoubleGenderValue(item.male, item.female));
    }

    // Make sure that the deaths table is zero based!
    auto deaths = ParameterLookup{};
    auto offset = entity.death_weight.front().value;
    for (const auto &item : entity.death_weight) {
        deaths.emplace(item.value - offset, DoubleGenderValue(item.male, item.female));
    }

    return {entity.at_time, distribution, survival, deaths};
}

LmsDefinition StoreConverter::to_lms_definition(const std::vector<core::LmsDataRow> &dataset) {
    auto lms_dataset = LmsDataset{};
    for (const auto &row : dataset) {
        lms_dataset[row.age][row.gender] =
            LmsRecord{.lambda = row.lambda, .mu = row.mu, .sigma = row.sigma};
    }

    return {std::move(lms_dataset)};
}
} // namespace hgps::detail
