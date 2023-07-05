#include "model_parser.h"

#include "csvparser.h"
#include "jsonparser.h"

#include "HealthGPS.Core/scoped_timer.h"

#include <fmt/color.h>
#include <fmt/core.h>
#include <fstream>

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

std::shared_ptr<hgps::HierarchicalLinearModelDefinition>
load_static_risk_model_definition(const poco::json &opt) {
    MEASURE_FUNCTION();
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

    return std::make_shared<hgps::HierarchicalLinearModelDefinition>(std::move(models),
                                                                     std::move(levels));
}

std::shared_ptr<hgps::LiteHierarchicalModelDefinition>
load_dynamic_risk_model_definition(const poco::json &opt) {
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

    return std::make_shared<hgps::LiteHierarchicalModelDefinition>(
        std::move(equations), std::move(variables), percentage);
}

std::shared_ptr<hgps::EnergyBalanceModelDefinition>
load_newebm_risk_model_definition(const poco::json &opt, const poco::SettingsInfo &settings) {
    MEASURE_FUNCTION();
    std::unordered_map<hgps::core::Identifier, double> energy_equation;
    std::unordered_map<hgps::core::Identifier, std::map<hgps::core::Identifier, double>>
        nutrient_equations;
    std::unordered_map<hgps::core::Gender, std::vector<double>> age_mean_height;

    // Save nutrient -> energy equation.
    for (const auto &nutrient : opt["Nutrients"]) {
        auto nutrient_key = nutrient["Name"].get<hgps::core::Identifier>();
        auto nutrient_energy = nutrient["Energy"].get<double>();
        energy_equation[nutrient_key] = nutrient_energy;
    }

    // Save food -> nutrient equations.
    for (const auto &food : opt["Foods"]) {
        auto food_key = food["Name"].get<hgps::core::Identifier>();
        auto food_nutrients = food["Nutrients"].get<std::map<std::string, double>>();

        for (const auto &nutrient : opt["Nutrients"]) {
            auto nutrient_key = nutrient["Name"].get<hgps::core::Identifier>();

            if (food_nutrients.contains(nutrient_key.to_string())) {
                double val = food_nutrients.at(nutrient_key.to_string());
                nutrient_equations[food_key][nutrient_key] = val;
            }
        }
    }

    // Save M/F average heights for age.
    auto male_height = opt["AgeMeanHeight"]["Male"].get<std::vector<double>>();
    age_mean_height.emplace(hgps::core::Gender::male, std::move(male_height));
    auto female_height = opt["AgeMeanHeight"]["Female"].get<std::vector<double>>();
    age_mean_height.emplace(hgps::core::Gender::female, std::move(female_height));

    return std::make_shared<hgps::EnergyBalanceModelDefinition>(
        std::move(energy_equation), std::move(nutrient_equations), std::move(age_mean_height));
}

void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const poco::ModellingInfo &info,
                                            const poco::SettingsInfo &settings) {
    MEASURE_FUNCTION();
    hgps::HierarchicalModelType model_type;
    std::shared_ptr<hgps::RiskFactorModelDefinition> model_definition;

    for (auto &model : info.risk_factor_models) {
        const auto &model_filename = model.second;
        std::ifstream ifs(model_filename, std::ifstream::in);

        if (!ifs.good()) {
            throw std::invalid_argument(fmt::format("Model file: {} not found", model_filename));
        }

        // Get this model's name.
        poco::json parsed_json = poco::json::parse(ifs);
        std::string model_name = hgps::core::to_lower(parsed_json["ModelName"].get<std::string>());

        if (hgps::core::case_insensitive::equals(model.first, "static")) {
            // Load this static model with the appropriate loader.
            model_type = hgps::HierarchicalModelType::Static;
            if (hgps::core::case_insensitive::equals(model_name, "hlm")) {
                model_definition = load_static_risk_model_definition(parsed_json);
            } else {
                fmt::print(fg(fmt::color::red), "Static model name '{}' is not recognised.\n",
                           model_name);
            }
        } else if (hgps::core::case_insensitive::equals(model.first, "dynamic")) {
            // Load this dynamic model with the appropriate loader.
            model_type = hgps::HierarchicalModelType::Dynamic;
            if (hgps::core::case_insensitive::equals(model_name, "ebhlm")) {
                model_definition = load_dynamic_risk_model_definition(parsed_json);
            } else if (hgps::core::case_insensitive::equals(model_name, "newebm")) {
                model_definition = load_newebm_risk_model_definition(parsed_json, settings);
            } else {
                fmt::print(fg(fmt::color::red), "Dynamic model name '{}' is not recognised.\n",
                           model_name);
            }
        } else {
            throw std::invalid_argument(fmt::format("Unknown model type: {}", model.first));
        }

        repository.register_risk_factor_model_definition(model_type, std::move(model_definition));
    }

    auto adjustment = load_baseline_adjustments(info.baseline_adjustment);
    auto age_range =
        hgps::core::IntegerInterval(settings.age_range.front(), settings.age_range.back());
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
