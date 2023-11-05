#include "model_parser.h"
#include "configuration_parsing_helpers.h"
#include "csvparser.h"
#include "jsonparser.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/scoped_timer.h"

#include <Eigen/Cholesky>
#include <Eigen/Dense>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fstream>
#include <optional>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                                         \
    hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

namespace host {

hgps::BaselineAdjustment load_baseline_adjustments(const poco::BaselineInfo &info) {
    MEASURE_FUNCTION();
    const auto male_filename = info.file_names.at("factorsmean_male").string();
    const auto female_filename = info.file_names.at("factorsmean_female").string();
    auto data = hgps::RiskFactorSexAgeTable{};

    if (!hgps::core::case_insensitive::equals(info.format, "CSV")) {
        throw hgps::core::HgpsException{"Unsupported file format: " + info.format};
    }

    try {
        data.emplace_row(hgps::core::Gender::male,
                         load_baseline_from_csv(male_filename, info.delimiter));
        data.emplace_row(hgps::core::Gender::female,
                         load_baseline_from_csv(female_filename, info.delimiter));
    } catch (const std::runtime_error &ex) {
        throw hgps::core::HgpsException{fmt::format("Failed to parse adjustment file: {} or {}. {}",
                                                    male_filename, female_filename, ex.what())};
    }

    return hgps::BaselineAdjustment{std::move(data)};
}

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_static_risk_model_definition(const std::string &model_name, const poco::json &opt,
                                  const host::Configuration &config) {
    // Load this static model with the appropriate loader.
    if (hgps::core::case_insensitive::equals(model_name, "hlm")) {
        return load_hlm_risk_model_definition(opt);
    }
    if (hgps::core::case_insensitive::equals(model_name, "staticlinear")) {
        return load_staticlinear_risk_model_definition(opt, config);
    }

    throw hgps::core::HgpsException{
        fmt::format("Static model name '{}' not recognised", model_name)};
}

std::unique_ptr<hgps::StaticHierarchicalLinearModelDefinition>
load_hlm_risk_model_definition(const poco::json &opt) {
    MEASURE_FUNCTION();
    std::map<int, hgps::HierarchicalLevel> levels;
    std::unordered_map<hgps::core::Identifier, hgps::LinearModel> models;

    poco::HierarchicalModelInfo model_info;
    model_info.models = opt["models"].get<std::unordered_map<std::string, poco::LinearModelInfo>>();
    model_info.levels =
        opt["levels"].get<std::unordered_map<std::string, poco::HierarchicalLevelInfo>>();

    for (const auto &model_item : model_info.models) {
        const auto &at = model_item.second;

        std::unordered_map<hgps::core::Identifier, hgps::Coefficient> coeffs;
        for (const auto &pair : at.coefficients) {
            coeffs.emplace(hgps::core::Identifier(pair.first),
                           hgps::Coefficient{.value = pair.second.value,
                                             .pvalue = pair.second.pvalue,
                                             .tvalue = pair.second.tvalue,
                                             .std_error = pair.second.std_error});
        }

        models.emplace(
            hgps::core::Identifier(model_item.first),
            hgps::LinearModel{.coefficients = std::move(coeffs),
                              .residuals_standard_deviation = at.residuals_standard_deviation,
                              .rsquared = at.rsquared});
    }

    for (auto &level_item : model_info.levels) {
        auto &at = level_item.second;
        std::unordered_map<hgps::core::Identifier, int> col_names;
        auto variables_count = static_cast<int>(at.variables.size());
        for (int i = 0; i < variables_count; i++) {
            col_names.emplace(hgps::core::Identifier{at.variables[i]}, i);
        }

        levels.emplace(
            std::stoi(level_item.first),
            hgps::HierarchicalLevel{
                .variables = std::move(col_names),
                .transition = hgps::core::DoubleArray2D(at.transition.rows, at.transition.cols,
                                                        at.transition.data),

                .inverse_transition = hgps::core::DoubleArray2D(at.inverse_transition.rows,
                                                                at.inverse_transition.cols,
                                                                at.inverse_transition.data),

                .residual_distribution = hgps::core::DoubleArray2D(at.residual_distribution.rows,
                                                                   at.residual_distribution.cols,
                                                                   at.residual_distribution.data),

                .correlation = hgps::core::DoubleArray2D(at.correlation.rows, at.correlation.cols,
                                                         at.correlation.data),

                .variances = at.variances});
    }

    return std::make_unique<hgps::StaticHierarchicalLinearModelDefinition>(std::move(models),
                                                                           std::move(levels));
}

std::unique_ptr<hgps::StaticLinearModelDefinition>
load_staticlinear_risk_model_definition(const poco::json &opt, const host::Configuration &config) {
    MEASURE_FUNCTION();

    // Risk factor linear models and correlation matrix.
    std::vector<hgps::LinearModelParams> risk_factor_models;
    const auto correlations_file_info =
        host::get_file_info(opt["RiskFactorCorrelationFile"], config.root_path);
    const auto correlations_table = load_datatable_from_csv(correlations_file_info);
    Eigen::MatrixXd correlations{correlations_table.num_rows(), correlations_table.num_columns()};

    for (size_t i = 0; i < opt["RiskFactorModels"].size(); i++) {
        // Risk factor model.
        const auto &factor = opt["RiskFactorModels"][i];
        hgps::LinearModelParams model;
        model.name = factor["Name"].get<hgps::core::Identifier>();
        model.intercept = factor["Intercept"].get<double>();
        model.coefficients =
            factor["Coefficients"].get<std::unordered_map<hgps::core::Identifier, double>>();

        // Check correlation matrix column name matches risk factor name.
        auto column_name = hgps::core::Identifier{correlations_table.column(i).name()};
        if (model.name != column_name) {
            throw hgps::core::HgpsException{
                fmt::format("Risk factor {} name ({}) does not match correlation matrix "
                            "column {} name ({})",
                            i, model.name.to_string(), i, column_name.to_string())};
        }

        // Write data structures.
        for (size_t j = 0; j < correlations_table.num_rows(); j++) {
            correlations(i, j) = std::any_cast<double>(correlations_table.column(i).value(j));
        }
        risk_factor_models.emplace_back(std::move(model));
    }

    // Risk factor mean values by sex and age.
    const poco::BaselineInfo &baseline_info = config.modelling.baseline_adjustment;
    const std::string male_filename = baseline_info.file_names.at("factorsmean_male").string();
    const std::string female_filename = baseline_info.file_names.at("factorsmean_female").string();
    auto risk_factor_means = hgps::RiskFactorSexAgeTable{};

    if (!hgps::core::case_insensitive::equals(baseline_info.format, "CSV")) {
        throw hgps::core::HgpsException{"Unsupported file format: " + baseline_info.format};
    }

    try {
        risk_factor_means.emplace_row(
            hgps::core::Gender::male,
            load_baseline_from_csv(male_filename, baseline_info.delimiter));
        risk_factor_means.emplace_row(
            hgps::core::Gender::female,
            load_baseline_from_csv(female_filename, baseline_info.delimiter));
    } catch (const std::runtime_error &ex) {
        throw hgps::core::HgpsException{fmt::format("Failed to parse adjustment file: {} or {}. {}",
                                                    male_filename, female_filename, ex.what())};
    }

    // Check means are defined for all risk factors.
    for (const hgps::LinearModelParams &model : risk_factor_models) {
        if (!risk_factor_means.at(hgps::core::Gender::male).contains(model.name)) {
            throw hgps::core::HgpsException{
                fmt::format("'{}' not defined in male factor means.", model.name.to_string())};
        }
        if (!risk_factor_means.at(hgps::core::Gender::female).contains(model.name)) {
            throw hgps::core::HgpsException{
                fmt::format("'{}' not defined in female factor means.", model.name.to_string())};
        }
    }

    // Check correlation matrix column count matches risk factor count.
    if (opt["RiskFactorModels"].size() != correlations_table.num_columns()) {
        throw hgps::core::HgpsException{
            fmt::format("Risk factor count ({}) does not match correlation "
                        "matrix column count ({})",
                        opt["RiskFactorModels"].size(), correlations_table.num_columns())};
    }

    // Compute Cholesky decomposition of correlation matrix.
    auto cholesky = Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{correlations}.matrixL()};

    return std::make_unique<hgps::StaticLinearModelDefinition>(
        std::move(risk_factor_models), std::move(risk_factor_means), std::move(cholesky));
}

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_dynamic_risk_model_definition(const std::string &model_name, const poco::json &opt,
                                   const host::Configuration &config) {
    // Load this dynamic model with the appropriate loader.
    if (hgps::core::case_insensitive::equals(model_name, "ebhlm")) {
        return load_ebhlm_risk_model_definition(opt);
    }
    if (hgps::core::case_insensitive::equals(model_name, "kevinhall")) {
        return load_kevinhall_risk_model_definition(opt, config);
    }

    throw hgps::core::HgpsException{
        fmt::format("Dynamic model name '{}' is not recognised.", model_name)};
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unique_ptr<hgps::DynamicHierarchicalLinearModelDefinition>
load_ebhlm_risk_model_definition(const poco::json &opt) {
    MEASURE_FUNCTION();
    auto percentage = 0.05;
    std::map<hgps::core::Identifier, hgps::core::Identifier> variables;
    std::map<hgps::core::IntegerInterval, hgps::AgeGroupGenderEquation> equations;

    auto info = poco::LiteHierarchicalModelInfo{};
    opt["BoundaryPercentage"].get_to(info.percentage);
    if (info.percentage > 0.0 && info.percentage < 1.0) {
        percentage = info.percentage;
    } else {
        throw hgps::core::HgpsException{
            fmt::format("Boundary percentage outside range (0, 1): {}", info.percentage)};
    }

    info.variables = opt["Variables"].get<std::vector<poco::VariableInfo>>();
    for (const auto &it : opt["Equations"].items()) {
        const auto &age_key = it.key();
        info.equations.emplace(
            age_key, std::map<std::string, std::vector<poco::FactorDynamicEquationInfo>>());

        for (const auto &sit : it.value().items()) {
            const auto &gender_key = sit.key();
            const auto &gender_funcs =
                sit.value().get<std::vector<poco::FactorDynamicEquationInfo>>();
            info.equations.at(age_key).emplace(gender_key, gender_funcs);
        }
    }

    for (const auto &item : info.variables) {
        variables.emplace(hgps::core::Identifier{item.name}, hgps::core::Identifier{item.factor});
    }

    for (const auto &age_grp : info.equations) {
        auto limits = hgps::core::split_string(age_grp.first, "-");
        auto age_key =
            hgps::core::IntegerInterval(std::stoi(limits[0].data()), std::stoi(limits[1].data()));
        auto age_equations = hgps::AgeGroupGenderEquation{.age_group = age_key};
        for (const auto &gender : age_grp.second) {

            if (hgps::core::case_insensitive::equals("male", gender.first)) {
                for (const auto &func : gender.second) {
                    auto function = hgps::FactorDynamicEquation{.name = func.name};
                    function.residuals_standard_deviation = func.residuals_standard_deviation;
                    for (const auto &coeff : func.coefficients) {
                        function.coefficients.emplace(hgps::core::to_lower(coeff.first),
                                                      coeff.second);
                    }

                    age_equations.male.emplace(hgps::core::to_lower(func.name), function);
                }
            } else if (hgps::core::case_insensitive::equals("female", gender.first)) {
                for (const auto &func : gender.second) {
                    auto function = hgps::FactorDynamicEquation{.name = func.name};
                    function.residuals_standard_deviation = func.residuals_standard_deviation;
                    for (const auto &coeff : func.coefficients) {
                        function.coefficients.emplace(hgps::core::to_lower(coeff.first),
                                                      coeff.second);
                    }

                    age_equations.female.emplace(hgps::core::to_lower(func.name), function);
                }
            } else {
                throw hgps::core::HgpsException{
                    fmt::format("Unknown model gender type: {}", gender.first)};
            }
        }

        equations.emplace(age_key, std::move(age_equations));
    }

    return std::make_unique<hgps::DynamicHierarchicalLinearModelDefinition>(
        std::move(equations), std::move(variables), percentage);
}
// NOLINTEND(readability-function-cognitive-complexity)

std::unique_ptr<hgps::KevinHallModelDefinition>
load_kevinhall_risk_model_definition(const poco::json &opt, const host::Configuration &config) {
    MEASURE_FUNCTION();

    // Nutrient groups.
    std::unordered_map<hgps::core::Identifier, double> energy_equation;
    std::unordered_map<hgps::core::Identifier, hgps::core::DoubleInterval> nutrient_ranges;
    for (const auto &nutrient : opt["Nutrients"]) {
        auto nutrient_key = nutrient["Name"].get<hgps::core::Identifier>();
        nutrient_ranges[nutrient_key] = nutrient["Range"].get<hgps::core::DoubleInterval>();
        energy_equation[nutrient_key] = nutrient["Energy"].get<double>();
    }

    // Food groups.
    std::unordered_map<hgps::core::Identifier, std::map<hgps::core::Identifier, double>>
        nutrient_equations;
    std::unordered_map<hgps::core::Identifier, std::optional<double>> food_prices;
    for (const auto &food : opt["Foods"]) {
        auto food_key = food["Name"].get<hgps::core::Identifier>();
        food_prices[food_key] = food["Price"].get<std::optional<double>>();
        auto food_nutrients = food["Nutrients"].get<std::map<hgps::core::Identifier, double>>();

        for (const auto &nutrient : opt["Nutrients"]) {
            auto nutrient_key = nutrient["Name"].get<hgps::core::Identifier>();

            if (food_nutrients.contains(nutrient_key.to_string())) {
                double val = food_nutrients.at(nutrient_key.to_string());
                nutrient_equations[food_key][nutrient_key] = val;
            }
        }
    }

    // Foods nutrition data table.
    const auto food_data_file_info = host::get_file_info(opt["FoodsDataFile"], config.root_path);
    const auto food_data_table = load_datatable_from_csv(food_data_file_info);

    // Rural sector prevalence for age groups and sex.
    std::unordered_map<hgps::core::Identifier, std::unordered_map<hgps::core::Gender, double>>
        rural_prevalence;
    for (const auto &age_group : opt["RuralPrevalence"]) {
        auto age_group_name = age_group["Name"].get<hgps::core::Identifier>();
        rural_prevalence[age_group_name] = {{hgps::core::Gender::female, age_group["Female"]},
                                            {hgps::core::Gender::male, age_group["Male"]}};
    }

    // Income models for different income classifications.
    std::vector<hgps::LinearModelParams> income_models;
    for (const auto &factor : opt["IncomeModels"]) {
        hgps::LinearModelParams model;
        model.name = factor["Name"].get<hgps::core::Identifier>();
        model.intercept = factor["Intercept"].get<double>();
        model.coefficients =
            factor["Coefficients"].get<std::unordered_map<hgps::core::Identifier, double>>();
        income_models.emplace_back(std::move(model));
    }

    // Load M/F average heights for age.
    std::unordered_map<hgps::core::Gender, std::vector<double>> age_mean_height;
    const auto max_age = static_cast<size_t>(config.settings.age_range.upper());
    auto male_height = opt["AgeMeanHeight"]["Male"].get<std::vector<double>>();
    auto female_height = opt["AgeMeanHeight"]["Female"].get<std::vector<double>>();
    if (male_height.size() <= max_age) {
        throw hgps::core::HgpsException{"AgeMeanHeight (Male) does not cover complete age range"};
    }
    if (female_height.size() <= max_age) {
        throw hgps::core::HgpsException{"AgeMeanHeight (Female) does not cover complete age range"};
    }
    age_mean_height.emplace(hgps::core::Gender::male, std::move(male_height));
    age_mean_height.emplace(hgps::core::Gender::female, std::move(female_height));

    return std::make_unique<hgps::KevinHallModelDefinition>(
        std::move(energy_equation), std::move(nutrient_ranges), std::move(nutrient_equations),
        std::move(food_prices), std::move(rural_prevalence), std::move(income_models),
        std::move(age_mean_height));
}

std::pair<hgps::RiskFactorModelType, std::unique_ptr<hgps::RiskFactorModelDefinition>>
load_risk_model_definition(const std::string &model_type, const poco::json &opt,
                           const host::Configuration &config) {
    // Get model name from JSON
    const auto model_name = hgps::core::to_lower(opt["ModelName"].get<std::string>());

    // Load appropriate model
    if (hgps::core::case_insensitive::equals(model_type, "static")) {
        return std::make_pair(hgps::RiskFactorModelType::Static,
                              load_static_risk_model_definition(model_name, opt, config));
    }
    if (hgps::core::case_insensitive::equals(model_type, "dynamic")) {
        return std::make_pair(hgps::RiskFactorModelType::Dynamic,
                              load_dynamic_risk_model_definition(model_name, opt, config));
    }

    throw hgps::core::HgpsException{fmt::format("Unknown model type: {}", model_type)};
}

poco::json load_json(const std::filesystem::path &model_path) {
    std::ifstream ifs(model_path, std::ifstream::in);
    if (!ifs.good()) {
        throw hgps::core::HgpsException{
            fmt::format("Model file: {} not found", model_path.string())};
    }

    return poco::json::parse(ifs);
}

void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const host::Configuration &config) {
    MEASURE_FUNCTION();

    for (const auto &model : config.modelling.risk_factor_models) {
        // Load file and parse JSON
        const auto opt = load_json(model.second);

        // Load appropriate dynamic/static model
        auto [model_type, model_definition] = load_risk_model_definition(model.first, opt, config);

        // Register model in cache
        repository.register_risk_factor_model_definition(model_type, std::move(model_definition));
    }

    auto adjustment = load_baseline_adjustments(config.modelling.baseline_adjustment);
    auto max_age = static_cast<std::size_t>(config.settings.age_range.upper());
    for (const auto &table : adjustment.values) {
        for (const auto &item : table.second) {
            if (item.second.size() <= max_age) {
                throw hgps::core::HgpsException{
                    fmt::format("Baseline adjustment file must cover the required age range: [{}].",
                                config.settings.age_range.to_string())};
            }
        }
    }

    repository.register_baseline_adjustment_definition(std::move(adjustment));
}

} // namespace host
