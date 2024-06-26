#include "model_parser.h"
#include "configuration_parsing_helpers.h"
#include "csvparser.h"
#include "jsonparser.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/scoped_timer.h"

#include <Eigen/Cholesky>
#include <Eigen/Dense>
#include <algorithm>
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

namespace hgps::input {

hgps::RiskFactorSexAgeTable load_risk_factor_expected(const Configuration &config) {
    MEASURE_FUNCTION();

    const auto &info = config.modelling.baseline_adjustment;
    if (!hgps::core::case_insensitive::equals(info.format, "CSV")) {
        throw hgps::core::HgpsException{"Unsupported file format: " + info.format};
    }

    auto table = hgps::RiskFactorSexAgeTable{};
    const auto male_filename = info.file_names.at("factorsmean_male").string();
    const auto female_filename = info.file_names.at("factorsmean_female").string();
    try {
        table.emplace_row(hgps::core::Gender::male,
                          load_baseline_from_csv(male_filename, info.delimiter));
        table.emplace_row(hgps::core::Gender::female,
                          load_baseline_from_csv(female_filename, info.delimiter));
    } catch (const std::runtime_error &ex) {
        throw hgps::core::HgpsException{fmt::format("Failed to parse adjustment file: {} or {}. {}",
                                                    male_filename, female_filename, ex.what())};
    }

    const auto max_age = static_cast<std::size_t>(config.settings.age_range.upper());
    for (const auto &sex : table) {
        for (const auto &factor : sex.second) {
            if (factor.second.size() <= max_age) {
                throw hgps::core::HgpsException{
                    fmt::format("Baseline adjustment file must cover the required age range: [{}].",
                                config.settings.age_range.to_string())};
            }
        }
    }

    return table;
}

std::unique_ptr<hgps::DummyModelDefinition>
load_dummy_model_definition(hgps::RiskFactorModelType type, const nlohmann::json &opt) {
    MEASURE_FUNCTION();

    // Get dummy model parameters
    std::vector<hgps::core::Identifier> names;
    std::vector<double> values;
    std::vector<double> policy;
    std::vector<int> policy_start;
    for (const auto &[key, json_params] : opt["ModelParameters"].items()) {
        names.emplace_back(key);
        values.emplace_back(json_params["Value"].get<double>());
        policy.emplace_back(json_params["Policy"].get<double>());
        policy_start.emplace_back(json_params["PolicyStart"].get<int>());
    }

    return std::make_unique<hgps::DummyModelDefinition>(type, std::move(names), std::move(values),
                                                        std::move(policy), std::move(policy_start));
}

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_static_risk_model_definition(const std::string &model_name, const nlohmann::json &opt,
                                  const Configuration &config) {
    // Load this static model with the appropriate loader.
    if (model_name == "dummy") {
        return load_dummy_model_definition(hgps::RiskFactorModelType::Static, opt);
    }
    if (model_name == "hlm") {
        return load_hlm_risk_model_definition(opt);
    }
    if (model_name == "staticlinear") {
        return load_staticlinear_risk_model_definition(opt, config);
    }

    throw hgps::core::HgpsException{
        fmt::format("Static model name '{}' not recognised", model_name)};
}

std::unique_ptr<hgps::StaticHierarchicalLinearModelDefinition>
load_hlm_risk_model_definition(const nlohmann::json &opt) {
    MEASURE_FUNCTION();
    std::map<int, hgps::HierarchicalLevel> levels;
    std::unordered_map<hgps::core::Identifier, hgps::LinearModel> models;

    HierarchicalModelInfo model_info;
    model_info.models = opt["models"].get<std::unordered_map<std::string, LinearModelInfo>>();
    model_info.levels = opt["levels"].get<std::unordered_map<std::string, HierarchicalLevelInfo>>();

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
load_staticlinear_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();

    // Risk factor correlation matrix.
    const auto correlation_file_info =
        hgps::input::get_file_info(opt["RiskFactorCorrelationFile"], config.root_path);
    const auto correlation_table = load_datatable_from_csv(correlation_file_info);
    Eigen::MatrixXd correlation{correlation_table.num_rows(), correlation_table.num_columns()};

    // Policy covariance matrix.
    const auto policy_covariance_file_info =
        hgps::input::get_file_info(opt["PolicyCovarianceFile"], config.root_path);
    const auto policy_covariance_table = load_datatable_from_csv(policy_covariance_file_info);
    Eigen::MatrixXd policy_covariance{policy_covariance_table.num_rows(),
                                      policy_covariance_table.num_columns()};

    // Risk factor and intervention policy: names, models, parameters and correlation/covariance.
    std::vector<hgps::core::Identifier> names;
    std::vector<hgps::LinearModelParams> models;
    std::vector<hgps::core::DoubleInterval> ranges;
    std::vector<double> lambda;
    std::vector<double> stddev;
    std::vector<hgps::LinearModelParams> policy_models;
    std::vector<hgps::core::DoubleInterval> policy_ranges;

    size_t i = 0;
    for (const auto &[key, json_params] : opt["RiskFactorModels"].items()) {
        names.emplace_back(key);

        // Risk factor model parameters.
        hgps::LinearModelParams model;
        model.intercept = json_params["Intercept"].get<double>();
        model.coefficients =
            json_params["Coefficients"].get<std::unordered_map<hgps::core::Identifier, double>>();

        // Check risk factor correlation matrix column name matches risk factor name.
        auto column_name = correlation_table.column(i).name();
        if (!hgps::core::case_insensitive::equals(key, column_name)) {
            throw hgps::core::HgpsException{
                fmt::format("Risk factor {} name ({}) does not match risk "
                            "factor correlation matrix column {} name ({})",
                            i, key, i, column_name)};
        }

        // Write risk factor data structures.
        models.emplace_back(std::move(model));
        ranges.emplace_back(json_params["Range"].get<hgps::core::DoubleInterval>());
        lambda.emplace_back(json_params["Lambda"].get<double>());
        stddev.emplace_back(json_params["StdDev"].get<double>());
        for (size_t j = 0; j < correlation_table.num_rows(); j++) {
            correlation(i, j) = std::any_cast<double>(correlation_table.column(i).value(j));
        }

        // Intervention policy model parameters.
        const auto &policy_json_params = json_params["Policy"];
        hgps::LinearModelParams policy_model;
        policy_model.intercept = policy_json_params["Intercept"].get<double>();
        policy_model.coefficients = policy_json_params["Coefficients"]
                                        .get<std::unordered_map<hgps::core::Identifier, double>>();
        policy_model.log_coefficients =
            policy_json_params["LogCoefficients"]
                .get<std::unordered_map<hgps::core::Identifier, double>>();

        // Check intervention policy covariance matrix column name matches risk factor name.
        auto policy_column_name = policy_covariance_table.column(i).name();
        if (!hgps::core::case_insensitive::equals(key, policy_column_name)) {
            throw hgps::core::HgpsException{
                fmt::format("Risk factor {} name ({}) does not match intervention "
                            "policy covariance matrix column {} name ({})",
                            i, key, i, policy_column_name)};
        }

        // Write intervention policy data structures.
        policy_models.emplace_back(std::move(policy_model));
        policy_ranges.emplace_back(policy_json_params["Range"].get<hgps::core::DoubleInterval>());
        for (size_t j = 0; j < policy_covariance_table.num_rows(); j++) {
            policy_covariance(i, j) =
                std::any_cast<double>(policy_covariance_table.column(i).value(j));
        }

        // Increment table column index.
        i++;
    }

    // Check risk factor correlation matrix column count matches risk factor count.
    if (opt["RiskFactorModels"].size() != correlation_table.num_columns()) {
        throw hgps::core::HgpsException{fmt::format("Risk factor count ({}) does not match risk "
                                                    "factor correlation matrix column count ({})",
                                                    opt["RiskFactorModels"].size(),
                                                    correlation_table.num_columns())};
    }

    // Compute Cholesky decomposition of the risk factor correlation matrix.
    auto cholesky = Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{correlation}.matrixL()};

    // Check intervention policy covariance matrix column count matches risk factor count.
    if (opt["RiskFactorModels"].size() != policy_covariance_table.num_columns()) {
        throw hgps::core::HgpsException{
            fmt::format("Risk factor count ({}) does not match intervention "
                        "policy covariance matrix column count ({})",
                        opt["RiskFactorModels"].size(), policy_covariance_table.num_columns())};
    }

    // Compute Cholesky decomposition of the intervention policy covariance matrix.
    auto policy_cholesky =
        Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{policy_covariance}.matrixL()};

    // Risk factor expected values by sex and age.
    hgps::RiskFactorSexAgeTable expected = load_risk_factor_expected(config);

    // Check expected values are defined for all risk factors.
    for (const auto &name : names) {
        if (!expected.at(hgps::core::Gender::male).contains(name)) {
            throw hgps::core::HgpsException{fmt::format(
                "'{}' is not defined in male risk factor expected values.", name.to_string())};
        }
        if (!expected.at(hgps::core::Gender::female).contains(name)) {
            throw hgps::core::HgpsException{fmt::format(
                "'{}' is not defined in female risk factor expected values.", name.to_string())};
        }
    }

    // Information speed of risk factor update.
    const double info_speed = opt["InformationSpeed"].get<double>();

    // Rural sector prevalence for age groups and sex.
    std::unordered_map<hgps::core::Identifier, std::unordered_map<hgps::core::Gender, double>>
        rural_prevalence;
    for (const auto &age_group : opt["RuralPrevalence"]) {
        auto age_group_name = age_group["Name"].get<hgps::core::Identifier>();
        rural_prevalence[age_group_name] = {{hgps::core::Gender::female, age_group["Female"]},
                                            {hgps::core::Gender::male, age_group["Male"]}};
    }

    // Income models for different income classifications.
    std::unordered_map<hgps::core::Income, hgps::LinearModelParams> income_models;
    for (const auto &[key, json_params] : opt["IncomeModels"].items()) {

        // Get income category.
        hgps::core::Income category;
        if (hgps::core::case_insensitive::equals(key, "Unknown")) {
            category = hgps::core::Income::unknown;
        } else if (hgps::core::case_insensitive::equals(key, "Low")) {
            category = hgps::core::Income::low;
        } else if (hgps::core::case_insensitive::equals(key, "Middle")) {
            category = hgps::core::Income::middle;
        } else if (hgps::core::case_insensitive::equals(key, "High")) {
            category = hgps::core::Income::high;
        } else {
            throw hgps::core::HgpsException(
                fmt::format("Income category {} is unrecognised.", key));
        }

        // Get income model parameters.
        hgps::LinearModelParams model;
        model.intercept = json_params["Intercept"].get<double>();
        model.coefficients =
            json_params["Coefficients"].get<std::unordered_map<hgps::core::Identifier, double>>();

        // Insert income model.
        income_models.emplace(category, std::move(model));
    }

    // Standard deviation of physical activity.
    const double physical_activity_stddev = opt["PhysicalActivityStdDev"].get<double>();

    return std::make_unique<hgps::StaticLinearModelDefinition>(
        std::move(expected), std::move(names), std::move(models), std::move(ranges),
        std::move(lambda), std::move(stddev), std::move(cholesky), std::move(policy_models),
        std::move(policy_ranges), std::move(policy_cholesky), info_speed,
        std::move(rural_prevalence), std::move(income_models), physical_activity_stddev);
}

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_dynamic_risk_model_definition(const std::string &model_name, const nlohmann::json &opt,
                                   const Configuration &config) {
    // Load this dynamic model with the appropriate loader.
    if (model_name == "dummy") {
        return load_dummy_model_definition(hgps::RiskFactorModelType::Dynamic, opt);
    }
    if (model_name == "ebhlm") {
        return load_ebhlm_risk_model_definition(opt, config);
    }
    if (model_name == "kevinhall") {
        return load_kevinhall_risk_model_definition(opt, config);
    }

    throw hgps::core::HgpsException{
        fmt::format("Dynamic model name '{}' is not recognised.", model_name)};
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unique_ptr<hgps::DynamicHierarchicalLinearModelDefinition>
load_ebhlm_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();

    auto info = LiteHierarchicalModelInfo{};

    auto percentage = 0.05;
    opt["BoundaryPercentage"].get_to(info.percentage);
    if (info.percentage > 0.0 && info.percentage < 1.0) {
        percentage = info.percentage;
    } else {
        throw hgps::core::HgpsException{
            fmt::format("Boundary percentage outside range (0, 1): {}", info.percentage)};
    }

    std::map<hgps::core::IntegerInterval, hgps::AgeGroupGenderEquation> equations;
    for (const auto &it : opt["Equations"].items()) {
        const auto &age_key = it.key();
        info.equations.emplace(age_key,
                               std::map<std::string, std::vector<FactorDynamicEquationInfo>>());

        for (const auto &sit : it.value().items()) {
            const auto &gender_key = sit.key();
            const auto &gender_funcs = sit.value().get<std::vector<FactorDynamicEquationInfo>>();
            info.equations.at(age_key).emplace(gender_key, gender_funcs);
        }
    }

    std::map<hgps::core::Identifier, hgps::core::Identifier> variables;
    info.variables = opt["Variables"].get<std::vector<VariableInfo>>();
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

    // Risk factor expected values by sex and age.
    hgps::RiskFactorSexAgeTable expected = load_risk_factor_expected(config);

    return std::make_unique<hgps::DynamicHierarchicalLinearModelDefinition>(
        std::move(expected), std::move(equations), std::move(variables), percentage);
}
// NOLINTEND(readability-function-cognitive-complexity)

std::unique_ptr<hgps::KevinHallModelDefinition>
load_kevinhall_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();

    // Risk factor expected values by sex and age.
    hgps::RiskFactorSexAgeTable expected = load_risk_factor_expected(config);

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

    // Weight quantiles.
    const auto weight_quantiles_table_F = load_datatable_from_csv(
        hgps::input::get_file_info(opt["WeightQuantiles"]["Female"], config.root_path));
    const auto weight_quantiles_table_M = load_datatable_from_csv(
        hgps::input::get_file_info(opt["WeightQuantiles"]["Male"], config.root_path));
    std::unordered_map<hgps::core::Gender, std::vector<double>> weight_quantiles = {
        {hgps::core::Gender::female, {}}, {hgps::core::Gender::male, {}}};
    weight_quantiles[hgps::core::Gender::female].reserve(weight_quantiles_table_F.num_rows());
    weight_quantiles[hgps::core::Gender::male].reserve(weight_quantiles_table_M.num_rows());
    for (size_t j = 0; j < weight_quantiles_table_F.num_rows(); j++) {
        weight_quantiles[hgps::core::Gender::female].push_back(
            std::any_cast<double>(weight_quantiles_table_F.column(0).value(j)));
    }
    for (size_t j = 0; j < weight_quantiles_table_M.num_rows(); j++) {
        weight_quantiles[hgps::core::Gender::male].push_back(
            std::any_cast<double>(weight_quantiles_table_M.column(0).value(j)));
    }
    for (auto &[unused, quantiles] : weight_quantiles) {
        std::sort(quantiles.begin(), quantiles.end());
    }

    // Energy Physical Activity quantiles.
    const auto epa_quantiles_table = load_datatable_from_csv(
        hgps::input::get_file_info(opt["EnergyPhysicalActivityQuantiles"], config.root_path));
    std::vector<double> epa_quantiles;
    epa_quantiles.reserve(epa_quantiles_table.num_rows());
    for (size_t j = 0; j < epa_quantiles_table.num_rows(); j++) {
        epa_quantiles.push_back(std::any_cast<double>(epa_quantiles_table.column(0).value(j)));
    }
    std::sort(epa_quantiles.begin(), epa_quantiles.end());

    // Load height model parameters.
    std::unordered_map<hgps::core::Gender, double> height_stddev = {
        {hgps::core::Gender::female, opt["HeightStdDev"]["Female"].get<double>()},
        {hgps::core::Gender::male, opt["HeightStdDev"]["Male"].get<double>()}};
    std::unordered_map<hgps::core::Gender, double> height_slope = {
        {hgps::core::Gender::female, opt["HeightSlope"]["Female"].get<double>()},
        {hgps::core::Gender::male, opt["HeightSlope"]["Male"].get<double>()}};

    return std::make_unique<hgps::KevinHallModelDefinition>(
        std::move(expected), std::move(energy_equation), std::move(nutrient_ranges),
        std::move(nutrient_equations), std::move(food_prices), std::move(weight_quantiles),
        std::move(epa_quantiles), std::move(height_stddev), std::move(height_slope));
}

std::pair<hgps::RiskFactorModelType, std::unique_ptr<hgps::RiskFactorModelDefinition>>
load_risk_model_definition(const std::string &model_type, const nlohmann::json &opt,
                           const Configuration &config) {
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

nlohmann::json load_json(const std::filesystem::path &model_path) {
    std::ifstream ifs(model_path, std::ifstream::in);
    if (!ifs.good()) {
        throw hgps::core::HgpsException{
            fmt::format("Model file: {} not found", model_path.string())};
    }

    return nlohmann::json::parse(ifs);
}

void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const Configuration &config) {
    MEASURE_FUNCTION();

    for (const auto &model : config.modelling.risk_factor_models) {
        // Load file and parse JSON
        const auto opt = load_json(model.second);

        // Load appropriate dynamic/static model
        auto [model_type, model_definition] = load_risk_model_definition(model.first, opt, config);

        // Register model in cache
        repository.register_risk_factor_model_definition(model_type, std::move(model_definition));
    }
}

} // namespace hgps::input
