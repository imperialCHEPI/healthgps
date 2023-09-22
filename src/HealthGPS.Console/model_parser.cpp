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
    auto data =
        std::map<hgps::core::Gender, std::map<hgps::core::Identifier, std::vector<double>>>{};

    if (!hgps::core::case_insensitive::equals(info.format, "CSV")) {
        throw hgps::core::HgpsException{"Unsupported file format: " + info.format};
    }

    try {
        data.emplace(hgps::core::Gender::male,
                     load_baseline_from_csv(male_filename, info.delimiter));
        data.emplace(hgps::core::Gender::female,
                     load_baseline_from_csv(female_filename, info.delimiter));
    } catch (const std::runtime_error &ex) {
        throw hgps::core::HgpsException{fmt::format("Failed to parse adjustment file: {} or {}. {}",
                                                    male_filename, female_filename, ex.what())};
    }

    return hgps::BaselineAdjustment{hgps::FactorAdjustmentTable{std::move(data)}};
}

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_static_risk_model_definition(const std::string &model_name, const poco::json &opt) {
    MEASURE_FUNCTION();
    if (!hgps::core::case_insensitive::equals(model_name, "hlm")) {
        throw hgps::core::HgpsException{
            fmt::format("Static model '{}' not recognised", model_name)};
    }

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

    return std::make_unique<hgps::HierarchicalLinearModelDefinition>(std::move(models),
                                                                     std::move(levels));
}

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_dynamic_risk_model_definition(const std::string &model_name, const poco::json &opt,
                                   const host::Configuration &config) {
    // Load this dynamic model with the appropriate loader.
    if (hgps::core::case_insensitive::equals(model_name, "ebhlm")) {
        return load_ebhlm_risk_model_definition(opt);
    }
    if (hgps::core::case_insensitive::equals(model_name, "newebm")) {
        return load_newebm_risk_model_definition(opt, config);
    }

    throw hgps::core::HgpsException{
        fmt::format("Dynamic model name '{}' is not recognised.", model_name)};
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unique_ptr<hgps::LiteHierarchicalModelDefinition>
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

    return std::make_unique<hgps::LiteHierarchicalModelDefinition>(
        std::move(equations), std::move(variables), percentage);
}
// NOLINTEND(readability-function-cognitive-complexity)

std::unique_ptr<hgps::EnergyBalanceModelDefinition>
load_newebm_risk_model_definition(const poco::json &opt, const host::Configuration &config) {
    MEASURE_FUNCTION();
    std::unordered_map<hgps::core::Identifier, double> energy_equation;
    std::unordered_map<hgps::core::Identifier, std::pair<double, double>> nutrient_ranges;
    std::unordered_map<hgps::core::Identifier, std::map<hgps::core::Identifier, double>>
        nutrient_equations;
    std::unordered_map<hgps::core::Identifier, std::optional<double>> food_prices;
    std::unordered_map<hgps::core::Gender, std::vector<double>> age_mean_height;

    // Nutrient groups.
    for (const auto &nutrient : opt["Nutrients"]) {
        auto nutrient_key = nutrient["Name"].get<hgps::core::Identifier>();
        nutrient_ranges[nutrient_key] = nutrient["Range"].get<std::pair<double, double>>();
        if (nutrient_ranges[nutrient_key].first > nutrient_ranges[nutrient_key].second) {
            throw hgps::core::HgpsException{
                fmt::format("Nutrient range is invalid: {}", nutrient_key.to_string())};
        }
        energy_equation[nutrient_key] = nutrient["Energy"].get<double>();
    }

    // Food groups.
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

    // Food names and correlation matrix.
    std::vector<hgps::core::Identifier> food_names;
    const auto food_correlations_file_info =
        host::get_file_info(opt["FoodsCorrelationFile"], config.root_path);
    const auto food_correlations_table = load_datatable_from_csv(food_correlations_file_info);
    Eigen::MatrixXd food_correlations{food_correlations_table.num_rows(),
                                      food_correlations_table.num_columns()};
    for (size_t col = 0; col < food_correlations_table.num_columns(); col++) {
        food_names.emplace_back(food_correlations_table.column(col).name());
        for (size_t row = 0; row < food_correlations_table.num_rows(); row++) {
            food_correlations(row, col) =
                std::any_cast<double>(food_correlations_table.column(col).value(row));
        }
    }
    auto food_cholesky = Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{food_correlations}.matrixL()};

    // Foods nutrition data table.
    const auto food_data_file_info = host::get_file_info(opt["FoodsDataFile"], config.root_path);
    const auto food_data_table = load_datatable_from_csv(food_data_file_info);

    // Load M/F average heights for age.
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

    return std::make_unique<hgps::EnergyBalanceModelDefinition>(
        std::move(energy_equation), std::move(nutrient_ranges), std::move(nutrient_equations),
        std::move(food_names), std::move(food_cholesky), std::move(food_prices),
        std::move(age_mean_height));
}

std::pair<hgps::HierarchicalModelType, std::unique_ptr<hgps::RiskFactorModelDefinition>>
load_risk_model_definition(const std::string &model_type, const poco::json &opt,
                           const host::Configuration &config) {
    // Get model name from JSON
    const auto model_name = hgps::core::to_lower(opt["ModelName"].get<std::string>());

    // Load appropriate model
    if (hgps::core::case_insensitive::equals(model_type, "static")) {
        return std::make_pair(hgps::HierarchicalModelType::Static,
                              load_static_risk_model_definition(model_name, opt));
    }
    if (hgps::core::case_insensitive::equals(model_type, "dynamic")) {
        return std::make_pair(hgps::HierarchicalModelType::Dynamic,
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
