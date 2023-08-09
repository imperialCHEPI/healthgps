#include "model_parser.h"
#include "csvparser.h"
#include "jsonparser.h"

#include "HealthGPS.Core/scoped_timer.h"

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
    auto &male_filename = info.file_names.at("factorsmean_male");
    auto &female_filename = info.file_names.at("factorsmean_female");

    try {

        if (hgps::core::case_insensitive::equals(info.format, "CSV")) {
            auto data = std::map<hgps::core::Gender,
                                 std::map<hgps::core::Identifier, std::vector<double>>>{};
            data.emplace(hgps::core::Gender::male,
                         load_baseline_from_csv(male_filename, info.delimiter));
            data.emplace(hgps::core::Gender::female,
                         load_baseline_from_csv(female_filename, info.delimiter));
            return hgps::BaselineAdjustment{hgps::FactorAdjustmentTable{std::move(data)}};
        } else {
            throw std::logic_error("Unsupported file format: " + info.format);
        }
    } catch (const std::exception &ex) {
        fmt::print(fg(fmt::color::red), "Failed to parse adjustment file: {} or {}. {}\n",
                   male_filename, female_filename, ex.what());
        throw;
    }
}

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_static_risk_model_definition(const std::string &model_name, const poco::json &opt) {
    MEASURE_FUNCTION();
    if (!hgps::core::case_insensitive::equals(model_name, "hlm")) {
        throw std::invalid_argument{fmt::format("Static model '{}' not recognised", model_name)};
    }

    std::map<int, hgps::HierarchicalLevel> levels;
    std::unordered_map<hgps::core::Identifier, hgps::LinearModel> models;

    poco::HierarchicalModelInfo model_info;
    model_info.models = opt["models"].get<std::unordered_map<std::string, poco::LinearModelInfo>>();
    model_info.levels =
        opt["levels"].get<std::unordered_map<std::string, poco::HierarchicalLevelInfo>>();

    for (const auto &model_item : model_info.models) {
        auto &at = model_item.second;

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

    throw std::invalid_argument{
        fmt::format("Dynamic model name '{}' is not recognised.", model_name)};
}

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
        throw std::invalid_argument(
            fmt::format("Boundary percentage outside range (0, 1): {}", info.percentage));
    }

    info.variables = opt["Variables"].get<std::vector<poco::VariableInfo>>();
    for (const auto &it : opt["Equations"].items()) {
        auto &age_key = it.key();
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
                throw std::invalid_argument(
                    fmt::format("Unknown model gender type: {}", gender.first));
            }
        }

        equations.emplace(age_key, std::move(age_equations));
    }

    return std::make_unique<hgps::LiteHierarchicalModelDefinition>(
        std::move(equations), std::move(variables), percentage);
}

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
            throw std::invalid_argument(
                fmt::format("Nutrient range is invalid: {}", nutrient_key.to_string()));
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

    // Foods nutrition data table.
    auto foods_data_table = hgps::core::DataTable{};
    auto foods_file_info = opt["FoodsDataFile"].get<poco::FileInfo>();
    std::filesystem::path file_path = foods_file_info.name;
    if (file_path.is_relative()) {
        file_path = config.root_path / file_path;
        foods_file_info.name = file_path.string();
    }
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error(
            fmt::format("Foods nutrition dataset file: {} not found.\n", file_path.string()));
    }
    load_datatable_from_csv(foods_data_table, foods_file_info);

    // Load M/F average heights for age.
    unsigned int max_age = config.settings.age_range.back();
    auto male_height = opt["AgeMeanHeight"]["Male"].get<std::vector<double>>();
    auto female_height = opt["AgeMeanHeight"]["Female"].get<std::vector<double>>();
    if (male_height.size() <= max_age) {
        throw std::invalid_argument("AgeMeanHeight (Male) does not cover complete age range");
    }
    if (female_height.size() <= max_age) {
        throw std::invalid_argument("AgeMeanHeight (Female) does not cover complete age range");
    }
    age_mean_height.emplace(hgps::core::Gender::male, std::move(male_height));
    age_mean_height.emplace(hgps::core::Gender::female, std::move(female_height));

    return std::make_unique<hgps::EnergyBalanceModelDefinition>(
        std::move(energy_equation), std::move(nutrient_ranges), std::move(nutrient_equations),
        std::move(food_prices), std::move(age_mean_height));
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

    throw std::invalid_argument(fmt::format("Unknown model type: {}", model_type));
}

poco::json load_json(const std::string &model_filename) {
    std::ifstream ifs(model_filename, std::ifstream::in);
    if (!ifs.good()) {
        throw std::invalid_argument(fmt::format("Model file: {} not found", model_filename));
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
    auto age_range = hgps::core::IntegerInterval(config.settings.age_range.front(),
                                                 config.settings.age_range.back());
    auto max_age = static_cast<std::size_t>(age_range.upper());
    for (const auto &table : adjustment.values) {
        for (const auto &item : table.second) {
            if (item.second.size() <= max_age) {
                fmt::print(fg(fmt::color::red),
                           "Baseline adjustment files data must cover age range: [{}].\n",
                           age_range.to_string());
                throw std::invalid_argument(
                    "Baseline adjustment file must cover the required age range.");
            }
        }
    }

    repository.register_baseline_adjustment_definition(std::move(adjustment));
}

} // namespace host
