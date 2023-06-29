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
using namespace hgps;
using namespace host::poco;

BaselineAdjustment load_baseline_adjustments(const BaselineInfo &info) {
    using namespace ::host;

    MEASURE_FUNCTION();
    auto &male_filename = info.file_names.at("factorsmean_male");
    auto &female_filename = info.file_names.at("factorsmean_female");

    try {

        if (core::case_insensitive::equals(info.format, "CSV")) {
            auto data = std::map<core::Gender, std::map<core::Identifier, std::vector<double>>>{};
            data.emplace(core::Gender::male, load_baseline_from_csv(male_filename, info.delimiter));
            data.emplace(core::Gender::female,
                         load_baseline_from_csv(female_filename, info.delimiter));
            return BaselineAdjustment{FactorAdjustmentTable{std::move(data)}};
        } else {
            throw std::logic_error("Unsupported file format: " + info.format);
        }
    } catch (const std::exception &ex) {
        fmt::print(fg(fmt::color::red), "Failed to parse adjustment file: {} or {}. {}\n",
                   male_filename, female_filename, ex.what());
        throw;
    }
}

std::shared_ptr<HierarchicalLinearModelDefinition>
load_static_risk_model_definition(const host::poco::json &opt) {
    using namespace detail;

    MEASURE_FUNCTION();
    std::map<int, HierarchicalLevel> levels;
    std::unordered_map<core::Identifier, LinearModel> models;

    HierarchicalModelInfo model_info;
    model_info.models = opt["models"].get<std::unordered_map<std::string, LinearModelInfo>>();
    model_info.levels = opt["levels"].get<std::unordered_map<std::string, HierarchicalLevelInfo>>();

    for (const auto &model_item : model_info.models) {
        auto &at = model_item.second;

        std::unordered_map<core::Identifier, Coefficient> coeffs;
        for (const auto &pair : at.coefficients) {
            coeffs.emplace(core::Identifier(pair.first),
                           Coefficient{.value = pair.second.value,
                                       .pvalue = pair.second.pvalue,
                                       .tvalue = pair.second.tvalue,
                                       .std_error = pair.second.std_error});
        }

        models.emplace(core::Identifier(model_item.first),
                       LinearModel{.coefficients = std::move(coeffs),
                                   .residuals_standard_deviation = at.residuals_standard_deviation,
                                   .rsquared = at.rsquared});
    }

    for (auto &level_item : model_info.levels) {
        auto &at = level_item.second;
        std::unordered_map<core::Identifier, int> col_names;
        auto variables_count = static_cast<int>(at.variables.size());
        for (int i = 0; i < variables_count; i++) {
            col_names.emplace(core::Identifier{at.variables[i]}, i);
        }

        levels.emplace(
            std::stoi(level_item.first),
            HierarchicalLevel{.variables = std::move(col_names),
                              .transition = core::DoubleArray2D(
                                  at.transition.rows, at.transition.cols, at.transition.data),

                              .inverse_transition = core::DoubleArray2D(at.inverse_transition.rows,
                                                                        at.inverse_transition.cols,
                                                                        at.inverse_transition.data),

                              .residual_distribution = core::DoubleArray2D(
                                  at.residual_distribution.rows, at.residual_distribution.cols,
                                  at.residual_distribution.data),

                              .correlation = core::DoubleArray2D(
                                  at.correlation.rows, at.correlation.cols, at.correlation.data),

                              .variances = at.variances});
    }

    return std::make_shared<HierarchicalLinearModelDefinition>(std::move(models),
                                                               std::move(levels));
}

std::shared_ptr<LiteHierarchicalModelDefinition>
load_dynamic_risk_model_definition(const host::poco::json &opt) {
    using namespace detail;

    MEASURE_FUNCTION();
    auto percentage = 0.05;
    std::map<core::Identifier, core::Identifier> variables;
    std::map<core::IntegerInterval, AgeGroupGenderEquation> equations;

    auto info = LiteHierarchicalModelInfo{};
    opt["BoundaryPercentage"].get_to(info.percentage);
    if (info.percentage > 0.0 && info.percentage < 1.0) {
        percentage = info.percentage;
    } else {
        throw std::invalid_argument(
            fmt::format("Boundary percentage outside range (0, 1): {}", info.percentage));
    }

    info.variables = opt["Variables"].get<std::vector<VariableInfo>>();
    for (const auto &it : opt["Equations"].items()) {
        auto &age_key = it.key();
        info.equations.emplace(age_key,
                               std::map<std::string, std::vector<FactorDynamicEquationInfo>>());

        for (const auto &sit : it.value().items()) {
            const auto &gender_key = sit.key();
            const auto &gender_funcs = sit.value().get<std::vector<FactorDynamicEquationInfo>>();
            info.equations.at(age_key).emplace(gender_key, gender_funcs);
        }
    }

    for (const auto &item : info.variables) {
        variables.emplace(core::Identifier{item.name}, core::Identifier{item.factor});
    }

    for (const auto &age_grp : info.equations) {
        auto limits = core::split_string(age_grp.first, "-");
        auto age_key =
            core::IntegerInterval(std::stoi(limits[0].data()), std::stoi(limits[1].data()));
        auto age_equations = AgeGroupGenderEquation{.age_group = age_key};
        for (const auto &gender : age_grp.second) {

            if (core::case_insensitive::equals("male", gender.first)) {
                for (const auto &func : gender.second) {
                    auto function = FactorDynamicEquation{.name = func.name};
                    function.residuals_standard_deviation = func.residuals_standard_deviation;
                    for (const auto &coeff : func.coefficients) {
                        function.coefficients.emplace(core::to_lower(coeff.first), coeff.second);
                    }

                    age_equations.male.emplace(core::to_lower(func.name), function);
                }
            } else if (core::case_insensitive::equals("female", gender.first)) {
                for (const auto &func : gender.second) {
                    auto function = FactorDynamicEquation{.name = func.name};
                    function.residuals_standard_deviation = func.residuals_standard_deviation;
                    for (const auto &coeff : func.coefficients) {
                        function.coefficients.emplace(core::to_lower(coeff.first), coeff.second);
                    }

                    age_equations.female.emplace(core::to_lower(func.name), function);
                }
            } else {
                throw std::invalid_argument(
                    fmt::format("Unknown model gender type: {}", gender.first));
            }
        }

        equations.emplace(age_key, std::move(age_equations));
    }

    return std::make_shared<LiteHierarchicalModelDefinition>(std::move(equations),
                                                             std::move(variables), percentage);
}

std::shared_ptr<EnergyBalanceModelDefinition>
load_newebm_risk_model_definition(const host::poco::json &opt) {
    MEASURE_FUNCTION();
    std::unordered_map<core::Identifier, double> energy_equation;
    std::unordered_map<core::Identifier, std::map<core::Identifier, double>> nutrient_equations;
    std::unordered_map<core::Gender, std::vector<double>> age_mean_height;

    // Save nutrient -> energy equation.
    for (const auto &nutrient : opt["Nutrients"]) {
        auto nutrient_str = nutrient["Name"].get<std::string>();
        auto nutrient_key = core::Identifier{nutrient_str};
        auto nutrient_energy = nutrient["Energy"].get<double>();
        energy_equation[nutrient_key] = nutrient_energy;
    }

    // Save food -> nutrient equations.
    for (const auto &food : opt["Foods"]) {
        auto food_str = food["Name"].get<std::string>();
        auto food_key = core::Identifier{food_str};
        auto food_nutrients = food["Nutrients"].get<std::map<std::string, double>>();

        for (const auto &nutrient : opt["Nutrients"]) {
            auto nutrient_str = nutrient["Name"].get<std::string>();
            auto nutrient_key = core::Identifier{nutrient_str};

            if (food_nutrients.contains(nutrient_str)) {
                double val = food_nutrients.at(nutrient_str);
                nutrient_equations[food_key][nutrient_key] = val;
            }
        }
    }

    // Save M/F average heights for age.
    auto male_height = opt["AgeMeanHeight"]["Male"].get<std::vector<double>>();
    age_mean_height.emplace(core::Gender::male, std::move(male_height));
    auto female_height = opt["AgeMeanHeight"]["Female"].get<std::vector<double>>();
    age_mean_height.emplace(core::Gender::female, std::move(female_height));

    return std::make_shared<EnergyBalanceModelDefinition>(
        std::move(energy_equation), std::move(nutrient_equations), std::move(age_mean_height));
}

void register_risk_factor_model_definitions(CachedRepository &repository, const ModellingInfo &info,
                                            const SettingsInfo &settings) {
    MEASURE_FUNCTION();
    HierarchicalModelType model_type;
    std::shared_ptr<RiskFactorModelDefinition> model_definition;

    for (auto &model : info.risk_factor_models) {
        const auto &model_filename = model.second;
        std::ifstream ifs(model_filename, std::ifstream::in);

        if (!ifs.good()) {
            throw std::invalid_argument(fmt::format("Model file: {} not found", model_filename));
        }

        // Get this model's name.
        host::poco::json parsed_json = json::parse(ifs);
        std::string model_name = core::to_lower(parsed_json["ModelName"].get<std::string>());

        if (core::case_insensitive::equals(model.first, "static")) {
            // Load this static model with the appropriate loader.
            model_type = HierarchicalModelType::Static;
            if (core::case_insensitive::equals(model_name, "hlm")) {
                model_definition = load_static_risk_model_definition(parsed_json);
            } else {
                fmt::print(fg(fmt::color::red), "Static model name '{}' is not recognised.\n",
                           model_name);
            }
        } else if (core::case_insensitive::equals(model.first, "dynamic")) {
            // Load this dynamic model with the appropriate loader.
            model_type = HierarchicalModelType::Dynamic;
            if (core::case_insensitive::equals(model_name, "ebhlm")) {
                model_definition = load_dynamic_risk_model_definition(parsed_json);
            } else if (core::case_insensitive::equals(model_name, "newebm")) {
                model_definition = load_newebm_risk_model_definition(parsed_json);
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
    auto age_range = core::IntegerInterval(settings.age_range.front(), settings.age_range.back());
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
