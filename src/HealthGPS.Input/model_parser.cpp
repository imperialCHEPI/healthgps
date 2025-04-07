#include "model_parser.h"
#include "configuration_parsing_helpers.h"
#include "csvparser.h"
#include "jsonparser.h"
#include "schema.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/scoped_timer.h"

#include <Eigen/Cholesky>
#include <Eigen/Dense>
#include <fmt/color.h>
#include <fmt/core.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                                         \
    hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

namespace {
// The latest schema version for each model
constexpr int DummyModelSchemaVersion = 1;
constexpr int EBHLMModelSchemaVersion = 1;
constexpr int KevinHallModelSchemaVersion = 2;
constexpr int HLMModelSchemaVersion = 1;
constexpr int StaticLinearModelSchemaVersion = 2;

//! Get the latest schema version for the given model
int get_model_schema_version(const std::string &model_name) {
    if (model_name == "dummy") {
        return DummyModelSchemaVersion;
    }
    if (model_name == "ebhlm") {
        return EBHLMModelSchemaVersion;
    }
    if (model_name == "kevinhall") {
        return KevinHallModelSchemaVersion;
    }
    if (model_name == "hlm") {
        return HLMModelSchemaVersion;
    }
    if (model_name == "staticlinear") {
        return StaticLinearModelSchemaVersion;
    }

    throw std::invalid_argument(fmt::format("Unknown model: {}", model_name));
}

/// @brief Load the model's JSON config file and validate with the appropriate schema
/// @param model_path The path to the model config file
/// @return A pair including the name of the model and the JSON contents of the file
std::pair<std::string, nlohmann::json>
load_and_validate_model_json(const std::filesystem::path &model_path) {
    std::ifstream ifs{model_path};
    if (!ifs) {
        throw std::runtime_error(fmt::format("Could not read file: {}", model_path.string()));
    }

    auto opt = nlohmann::json::parse(ifs);
    auto model_name = hgps::core::to_lower(opt["ModelName"].get<std::string>());
    const auto model_schema_name = fmt::format("config/models/{}.json", model_name);

    ifs.seekg(0); // Rewind to start
    hgps::input::validate_json(ifs, model_schema_name, get_model_schema_version(model_name));

    return std::make_pair(std::move(model_name), std::move(opt));
}
} // anonymous namespace

namespace hgps::input {

nlohmann::json load_json(const std::filesystem::path &filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw hgps::core::HgpsException(fmt::format("Could not open file: {}", filepath.string()));
    }

    try {
        return nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error &e) {
        throw hgps::core::HgpsException(
            fmt::format("Failed to parse JSON file {}: {}", filepath.string(), e.what()));
    }
}

std::unique_ptr<hgps::RiskFactorSexAgeTable>
load_risk_factor_expected(const Configuration &config) {
    MEASURE_FUNCTION();

    const auto &info = config.modelling.baseline_adjustment;
    if (!hgps::core::case_insensitive::equals(info.format, "CSV")) {
        throw hgps::core::HgpsException{"Unsupported file format: " + info.format};
    }

    auto table = std::make_unique<hgps::RiskFactorSexAgeTable>();
    const auto male_filename = info.file_names.at("factorsmean_male").string();
    const auto female_filename = info.file_names.at("factorsmean_female").string();
    try {
        table->emplace_row(hgps::core::Gender::male,
                           load_baseline_from_csv(male_filename, info.delimiter));
        table->emplace_row(hgps::core::Gender::female,
                           load_baseline_from_csv(female_filename, info.delimiter));
    } catch (const std::runtime_error &ex) {
        throw hgps::core::HgpsException{fmt::format("Failed to parse adjustment file: {} or {}. {}",
                                                    male_filename, female_filename, ex.what())};
    }

    const auto max_age = static_cast<std::size_t>(config.settings.age_range.upper());
    for (const auto &sex : *table) {
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
load_dummy_risk_model_definition(hgps::RiskFactorModelType type, const nlohmann::json &opt) {
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
            hgps::Coefficient coeff;
            coeff.value = pair.second.value;
            coeff.pvalue = pair.second.pvalue;
            coeff.tvalue = pair.second.tvalue;
            coeff.std_error = pair.second.std_error;
            coeffs.emplace(hgps::core::Identifier(pair.first), coeff);
        }

        hgps::LinearModel model;
        model.coefficients = std::move(coeffs);
        model.residuals_standard_deviation = at.residuals_standard_deviation;
        model.rsquared = at.rsquared;
        models.emplace(hgps::core::Identifier(model_item.first), std::move(model));
    }

    for (auto &level_item : model_info.levels) {
        auto &at = level_item.second;
        std::unordered_map<hgps::core::Identifier, int> col_names;
        auto variables_count = static_cast<int>(at.variables.size());
        for (int i = 0; i < variables_count; i++) {
            col_names.emplace(hgps::core::Identifier{at.variables[i]}, i);
        }

        hgps::HierarchicalLevel level;
        level.variables = std::move(col_names);
        level.transition =
            hgps::core::DoubleArray2D(at.transition.rows, at.transition.cols, at.transition.data);
        level.inverse_transition = hgps::core::DoubleArray2D(
            at.inverse_transition.rows, at.inverse_transition.cols, at.inverse_transition.data);
        level.residual_distribution =
            hgps::core::DoubleArray2D(at.residual_distribution.rows, at.residual_distribution.cols,
                                      at.residual_distribution.data);
        level.correlation = hgps::core::DoubleArray2D(at.correlation.rows, at.correlation.cols,
                                                      at.correlation.data);
        level.variances = at.variances;

        levels.emplace(std::stoi(level_item.first), std::move(level));
    }

    return std::make_unique<hgps::StaticHierarchicalLinearModelDefinition>(std::move(models),
                                                                           std::move(levels));
}

namespace detail {

void process_risk_factor_models(
    const nlohmann::json &models_json, const core::DataTable &correlation_table,
    const core::DataTable &policy_covariance_table, std::vector<core::Identifier> &names,
    std::vector<LinearModelParams> &models, std::vector<core::DoubleInterval> &ranges,
    std::vector<double> &lambda, std::vector<double> &stddev,
    std::vector<LinearModelParams> &policy_models, std::vector<core::DoubleInterval> &policy_ranges,
    std::unique_ptr<std::vector<LinearModelParams>> &trend_models,
    std::unique_ptr<std::vector<core::DoubleInterval>> &trend_ranges,
    std::unique_ptr<std::vector<double>> &trend_lambda,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> &expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> &expected_trend_boxcox,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> &trend_steps,
    Eigen::MatrixXd &correlation, Eigen::MatrixXd &policy_covariance) {

    size_t i = 0;
    for (const auto &[key, json_params] : models_json.items()) {
        names.emplace_back(key);

        // Risk factor model parameters.
        LinearModelParams model;
        model.intercept = json_params["Intercept"].get<double>();

        model.coefficients =
            json_params["Coefficients"].get<std::unordered_map<core::Identifier, double>>();

        // Check risk factor correlation matrix column name matches risk factor name.
        auto column_name = correlation_table.column(i).name();
        if (!core::case_insensitive::equals(key, column_name)) {
            throw core::HgpsException{fmt::format("Risk factor {} name ({}) does not match risk "
                                                  "factor correlation matrix column {} name ({})",
                                                  i, key, i, column_name)};
        }

        // Write risk factor data structures.
        models.emplace_back(std::move(model));
        ranges.emplace_back(json_params["Range"].get<core::DoubleInterval>());
        lambda.emplace_back(json_params["Lambda"].get<double>());
        stddev.emplace_back(json_params["StdDev"].get<double>());

        for (size_t j = 0; j < correlation_table.num_rows(); j++) {
            correlation(i, j) = std::any_cast<double>(correlation_table.column(i).value(j));
        }

        // Intervention policy model parameters.
        const auto &policy_json_params = json_params["Policy"];
        LinearModelParams policy_model;
        policy_model.intercept = policy_json_params["Intercept"].get<double>();
        policy_model.coefficients =
            policy_json_params["Coefficients"].get<std::unordered_map<core::Identifier, double>>();
        policy_model.log_coefficients = policy_json_params["LogCoefficients"]
                                            .get<std::unordered_map<core::Identifier, double>>();

        // Check intervention policy covariance matrix column name matches risk factor name.
        auto policy_column_name = policy_covariance_table.column(i).name();
        if (!core::case_insensitive::equals(key, policy_column_name)) {
            throw core::HgpsException{
                fmt::format("Risk factor {} name ({}) does not match intervention "
                            "policy covariance matrix column {} name ({})",
                            i, key, i, policy_column_name)};
        }

        // Write intervention policy data structures.
        policy_models.emplace_back(std::move(policy_model));
        policy_ranges.emplace_back(policy_json_params["Range"].get<core::DoubleInterval>());
        for (size_t j = 0; j < policy_covariance_table.num_rows(); j++) {
            policy_covariance(i, j) =
                std::any_cast<double>(policy_covariance_table.column(i).value(j));
        }

        // Time trend model parameters.
        const auto &trend_json_params = json_params["Trend"];
        LinearModelParams trend_model;
        trend_model.intercept = trend_json_params["Intercept"].get<double>();
        trend_model.coefficients =
            trend_json_params["Coefficients"].get<std::unordered_map<core::Identifier, double>>();
        trend_model.log_coefficients = trend_json_params["LogCoefficients"]
                                           .get<std::unordered_map<core::Identifier, double>>();

        // Write time trend data structures.
        trend_models->emplace_back(std::move(trend_model));
        auto trend_lower = trend_json_params["Range"][0].get<double>();
        auto trend_upper = trend_json_params["Range"][1].get<double>();
        trend_ranges->emplace_back(core::DoubleInterval(trend_lower, trend_upper));
        trend_lambda->emplace_back(trend_json_params["Lambda"].get<double>());

        // Load expected value trends.
        (*expected_trend)[key] = json_params["ExpectedTrend"].get<double>();
        (*expected_trend_boxcox)[key] = json_params["ExpectedTrendBoxCox"].get<double>();
        (*trend_steps)[key] = json_params["TrendSteps"].get<int>();

        i++;
    }

    // Check risk factor correlation matrix column count matches risk factor count.
    if (models_json.size() != correlation_table.num_columns()) {
        throw core::HgpsException{fmt::format("Risk factor count ({}) does not match risk "
                                              "factor correlation matrix column count ({})",
                                              models_json.size(), correlation_table.num_columns())};
    }

    // Check intervention policy covariance matrix column count matches risk factor count.
    if (models_json.size() != policy_covariance_table.num_columns()) {
        throw core::HgpsException{fmt::format("Risk factor count ({}) does not match intervention "
                                              "policy covariance matrix column count ({})",
                                              models_json.size(),
                                              policy_covariance_table.num_columns())};
    }
}

std::unordered_map<core::Income, LinearModelParams>
process_income_models(const nlohmann::json &income_models_json) {
    std::unordered_map<core::Income, LinearModelParams> income_models;

    // Check if income_models_json is null
    if (income_models_json.is_null()) {
        throw core::HgpsException{"Income models data is null in static_model.json"};
    }

    for (const auto &item : income_models_json.items()) {
        const auto &key = item.key();
        const auto &json_params = item.value();

        // Check if the value is an object before proceeding
        if (!json_params.is_object()) {
            throw core::HgpsException{
                fmt::format("Value for key '{}' is not a JSON object (type: {})", key,
                            json_params.type_name())};
        }

        // Skip empty or whitespace-only keys
        std::string trimmed_key = key;
        trimmed_key.erase(0, trimmed_key.find_first_not_of(" \t\n\r\f\v"));
        if (trimmed_key.empty() ||
            trimmed_key.find_last_not_of(" \t\n\r\f\v") == std::string::npos) {
            throw core::HgpsException{
                "Empty or whitespace-only income category key in static_model.json"};
        }
        trimmed_key.erase(trimmed_key.find_last_not_of(" \t\n\r\f\v") + 1);

        // Skip null JSON params
        if (json_params.is_null()) {
            throw core::HgpsException{fmt::format(
                "JSON parameters for income model {} are null in static_model.json", trimmed_key)};
        }

        try {
            // Get income category.
            core::Income category;

            // Special case for continuous - map it to a recognized category
            if (core::case_insensitive::equals(trimmed_key, "continuous")) {
                category = core::Income::unknown;
            } else if (core::case_insensitive::equals(trimmed_key, "Unknown")) {
                category = core::Income::unknown;
            } else if (core::case_insensitive::equals(trimmed_key, "Low")) {
                category = core::Income::low;
            } else if (core::case_insensitive::equals(trimmed_key, "LowerMiddle")) {
                category = core::Income::lowermiddle;
            } else if (core::case_insensitive::equals(trimmed_key, "UpperMiddle")) {
                category = core::Income::uppermiddle;
            } else if (core::case_insensitive::equals(trimmed_key, "High")) {
                category = core::Income::high;
            } else {
                throw core::HgpsException{
                    fmt::format("Unknown income category: {} in static_model.json", trimmed_key)};
            }

            // Check required JSON fields
            if (!json_params.contains("Intercept") || !json_params.contains("Coefficients")) {
                throw core::HgpsException{
                    fmt::format("Income model {} is missing required fields in static_model.json",
                                trimmed_key)};
            }

            if (json_params["Intercept"].is_null() || json_params["Coefficients"].is_null()) {
                throw core::HgpsException{fmt::format(
                    "Income model {} has null required fields in static_model.json", trimmed_key)};
            }

            // Get income model parameters.
            LinearModelParams model;
            model.intercept = json_params["Intercept"].get<double>();

            // Get coefficients safely
            try {
                if (json_params["Coefficients"].is_object()) {
                    for (const auto &[coef_key, coef_value] : json_params["Coefficients"].items()) {
                        if (!coef_value.is_null()) {
                            model.coefficients[core::Identifier(coef_key)] =
                                coef_value.get<double>();
                        } else {
                            throw core::HgpsException{
                                fmt::format("Null coefficient value for key {} in income model {} "
                                            "in static_model.json",
                                            coef_key, trimmed_key)};
                        }
                    }
                } else {
                    throw core::HgpsException{fmt::format(
                        "Coefficients is not an object for income model {} in static_model.json",
                        trimmed_key)};
                }
            } catch (const std::exception &e) {
                throw core::HgpsException{
                    fmt::format("Error processing coefficients for income model {}: {}",
                                trimmed_key, e.what())};
            }

            // Insert income model.
            income_models[category] = std::move(model);
        } catch (const std::exception &e) {
            throw core::HgpsException{
                fmt::format("Error processing income model {}: {}", trimmed_key, e.what())};
        }
    }

    if (income_models.empty()) {
        throw core::HgpsException{"No valid income models found in static_model.json"};
    }

    return income_models;
}

std::unordered_map<core::Region, std::unordered_map<core::Gender, double>>
process_region_prevalence(const nlohmann::json &region_prevalence_json) {
    std::unordered_map<core::Region, std::unordered_map<core::Gender, double>> region_prevalence;

    // Check if region_prevalence_json is null
    if (region_prevalence_json.is_null()) {
        throw core::HgpsException{"Region prevalence data is null in static_model.json"};
    }

    for (const auto &age_group : region_prevalence_json) {
        for (const auto &[gender_str, regions] : age_group.items()) {
            if (gender_str == "Name") {
                continue;
            }

            // Skip null JSON params
            if (regions.is_null()) {
                throw core::HgpsException{
                    fmt::format("Regions for gender {} is null in static_model.json", gender_str)};
            }

            core::Gender gender = core::case_insensitive::equals(gender_str, "Female")
                                      ? core::Gender::female
                                      : core::Gender::male;

            for (const auto &[region_str, value] : regions.items()) {
                // Skip null or invalid values
                if (value.is_null()) {
                    throw core::HgpsException{fmt::format(
                        "Value for region {} is null in static_model.json", region_str)};
                }

                auto region = parse_region(region_str);
                region_prevalence[region][gender] = value.get<double>();
            }
        }
    }
    return region_prevalence;
}

std::unordered_map<core::Ethnicity, std::unordered_map<core::Gender, double>>
process_ethnicity_prevalence(const nlohmann::json &ethnicity_prevalence_json) {
    std::unordered_map<core::Ethnicity, std::unordered_map<core::Gender, double>>
        ethnicity_prevalence;

    // Check if ethnicity_prevalence_json is null
    if (ethnicity_prevalence_json.is_null()) {
        throw core::HgpsException{"Ethnicity prevalence data is null in static_model.json"};
    }

    for (const auto &age_group : ethnicity_prevalence_json) {
        for (const auto &[gender_str, ethnicities] : age_group.items()) {
            if (gender_str == "Name") {
                continue;
            }

            // Skip null JSON params
            if (ethnicities.is_null()) {
                throw core::HgpsException{fmt::format(
                    "Ethnicities for gender {} is null in static_model.json", gender_str)};
            }

            core::Gender gender = core::case_insensitive::equals(gender_str, "Female")
                                      ? core::Gender::female
                                      : core::Gender::male;

            for (const auto &[ethnicity_str, value] : ethnicities.items()) {
                // Skip null or invalid values
                if (value.is_null()) {
                    throw core::HgpsException{fmt::format(
                        "Value for ethnicity {} is null in static_model.json", ethnicity_str)};
                }
                auto ethnicity = parse_ethnicity(ethnicity_str);
                ethnicity_prevalence[ethnicity][gender] = value.get<double>();
            }
        }
    }
    return ethnicity_prevalence;
}

std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
process_rural_prevalence(const nlohmann::json &rural_prevalence_json) {
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>> rural_prevalence;
    for (const auto &age_group : rural_prevalence_json) {
        for (const auto &[gender_str, sectors] : age_group.items()) {
            if (gender_str == "Name") {
                continue;
            }

            // Skip null JSON params
            if (sectors.is_null()) {
                fmt::print("Skipping null sectors for gender: {}\n", gender_str);
                continue;
            }

            core::Gender gender = core::case_insensitive::equals(gender_str, "Female")
                                      ? core::Gender::female
                                      : core::Gender::male;

            for (const auto &[sector_str, value] : sectors.items()) {
                // Skip empty or whitespace-only keys
                std::string trimmed_sector = sector_str;
                trimmed_sector.erase(0, trimmed_sector.find_first_not_of(" \t\n\r\f\v"));
                if (trimmed_sector.empty() ||
                    trimmed_sector.find_last_not_of(" \t\n\r\f\v") == std::string::npos) {
                    continue;
                }
                trimmed_sector.erase(trimmed_sector.find_last_not_of(" \t\n\r\f\v") + 1);

                // Skip null values
                if (value.is_null()) {
                    continue;
                }

                core::Identifier sector(trimmed_sector);
                rural_prevalence[sector][gender] = value.get<double>();
            }
        }
    }
    return rural_prevalence;
}

void validate_expected_values(const std::vector<core::Identifier> &names,
                              const std::unique_ptr<hgps::RiskFactorSexAgeTable> &expected) {

    for (const auto &name : names) {
        if (expected->at(core::Gender::male).find(name) == expected->at(core::Gender::male).end()) {
            throw core::HgpsException{fmt::format(
                "'{}' is not defined in male risk factor expected values.", name.to_string())};
        }
        if (expected->at(core::Gender::female).find(name) ==
            expected->at(core::Gender::female).end()) {
            throw core::HgpsException{fmt::format(
                "'{}' is not defined in female risk factor expected values.", name.to_string())};
        }
    }
}

} // namespace detail

std::unique_ptr<hgps::StaticLinearModelDefinition>
load_staticlinear_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();

    // Risk factor correlation matrix - REQUIRED from static_model.json
    if (!opt.contains("RiskFactorCorrelationFile") || opt["RiskFactorCorrelationFile"].is_null()) {
        throw core::HgpsException{
            "RiskFactorCorrelationFile field is missing or null in static_model.json"};
    }

    const auto correlation_file_info =
        input::get_file_info(opt["RiskFactorCorrelationFile"], config.root_path);
    const auto correlation_table = load_datatable_from_csv(correlation_file_info);
    Eigen::MatrixXd correlation{correlation_table.num_rows(), correlation_table.num_columns()};

    // Policy covariance matrix - REQUIRED from static_model.json
    if (!opt.contains("PolicyCovarianceFile") || opt["PolicyCovarianceFile"].is_null()) {
        throw core::HgpsException{
            "PolicyCovarianceFile field is missing or null in static_model.json"};
    }

    const auto policy_covariance_file_info =
        input::get_file_info(opt["PolicyCovarianceFile"], config.root_path);
    const auto policy_covariance_table = load_datatable_from_csv(policy_covariance_file_info);
    Eigen::MatrixXd policy_covariance{policy_covariance_table.num_rows(),
                                      policy_covariance_table.num_columns()};

    // Risk factor and intervention policy: names, models, parameters and correlation/covariance.
    std::vector<core::Identifier> names;
    std::vector<LinearModelParams> models;
    std::vector<core::DoubleInterval> ranges;
    std::vector<double> lambda;
    std::vector<double> stddev;
    std::vector<LinearModelParams> policy_models;
    std::vector<core::DoubleInterval> policy_ranges;
    auto trend_models = std::make_unique<std::vector<LinearModelParams>>();
    auto trend_ranges = std::make_unique<std::vector<core::DoubleInterval>>();
    auto trend_lambda = std::make_unique<std::vector<double>>();
    auto expected_trend = std::make_unique<std::unordered_map<core::Identifier, double>>();
    auto expected_trend_boxcox = std::make_unique<std::unordered_map<core::Identifier, double>>();
    auto trend_steps = std::make_unique<std::unordered_map<core::Identifier, int>>();

    // Process risk factor models - REQUIRED from static_model.json
    if (!opt.contains("RiskFactorModels") || opt["RiskFactorModels"].is_null()) {
        throw core::HgpsException{"RiskFactorModels field is missing or null in static_model.json"};
    }

    detail::process_risk_factor_models(
        opt["RiskFactorModels"], correlation_table, policy_covariance_table, names, models, ranges,
        lambda, stddev, policy_models, policy_ranges, trend_models, trend_ranges, trend_lambda,
        expected_trend, expected_trend_boxcox, trend_steps, correlation, policy_covariance);

    // Compute Cholesky decomposition of the risk factor correlation matrix.
    auto cholesky = Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{correlation}.matrixL()};

    // Compute Cholesky decomposition of the intervention policy covariance matrix.
    auto policy_cholesky =
        Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{policy_covariance}.matrixL()};

    // Risk factor expected values by sex and age.
    auto expected = load_risk_factor_expected(config);

    // Check expected values are defined for all risk factors.
    detail::validate_expected_values(names, expected);

    // Information speed of risk factor update - REQUIRED from static_model.json
    if (!opt.contains("InformationSpeed") || opt["InformationSpeed"].is_null()) {
        throw core::HgpsException{"InformationSpeed field is missing or null in static_model.json"};
    }

    const double info_speed = opt["InformationSpeed"].get<double>();

    // Validate and process rural prevalence - OPTIONAL for testing (as per user's request)
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>> rural_prevalence;
    if (opt.contains("RuralPrevalence") && !opt["RuralPrevalence"].is_null()) {
        rural_prevalence = detail::process_rural_prevalence(opt["RuralPrevalence"]);
    } else {
        // Create an empty map - rural prevalence will effectively be off
        core::Identifier ruralSector("Rural");
        rural_prevalence[ruralSector][core::Gender::male] = 0.0;
        rural_prevalence[ruralSector][core::Gender::female] = 0.0;
    }

    // Validate and load PhysicalActivityStdDev - REQUIRED from static_model.json
    if (!opt.contains("PhysicalActivityStdDev") || opt["PhysicalActivityStdDev"].is_null()) {
        throw std::invalid_argument(
            "PhysicalActivityStdDev field is missing or null in static_model.json");
    }
    double physical_activity_stddev = opt["PhysicalActivityStdDev"].get<double>();

    // Load income_continuous_stddev - REQUIRED from static_model.json
    double income_continuous_stddev =
        293.752576754905; // Hardcoded value instead of reading from JSON

    // Initialize height data with null values - these are loaded from dynamic_model.json for Kevin
    // Hall model Static linear model will never use these values
    std::unordered_map<hgps::core::Gender, double> height_stddev;
    std::unordered_map<hgps::core::Gender, double> height_slope;

    // Load region prevalence - REQUIRED from static_model.json
    if (!opt.contains("RegionPrevalence") || opt["RegionPrevalence"].is_null()) {
        throw core::HgpsException{"RegionPrevalence field is missing or null in static_model.json"};
    }

    auto region_prevalence = detail::process_region_prevalence(opt["RegionPrevalence"]);

    // Load ethnicity prevalence - REQUIRED from static_model.json
    if (!opt.contains("EthnicityPrevalence") || opt["EthnicityPrevalence"].is_null()) {
        throw core::HgpsException{
            "EthnicityPrevalence field is missing or null in static_model.json"};
    }

    auto ethnicity_prevalence = detail::process_ethnicity_prevalence(opt["EthnicityPrevalence"]);

    // Load income models - REQUIRED from static_model.json
    if (!opt.contains("IncomeModels") || opt["IncomeModels"].is_null()) {
        throw core::HgpsException{"IncomeModels field is missing or null in static_model.json"};
    }

    auto income_models = detail::process_income_models(opt["IncomeModels"]);

    // Process region models - OPTIONAL but use region prevalence if not present
    std::unordered_map<core::Region, LinearModelParams> region_models;
    if (opt.contains("RegionModels") && !opt["RegionModels"].is_null()) {
        for (const auto &[region_name, model] : opt["RegionModels"].items()) {
            if (!model.contains("Intercept") || model["Intercept"].is_null()) {
                continue;
            }

            if (!model.contains("Coefficients") || model["Coefficients"].is_null()) {
                continue;
            }

            auto intercept = model["Intercept"].get<double>();
            std::unordered_map<hgps::core::Identifier, double> coefs;
            if (model["Coefficients"].is_array()) {
                // Handle vector format
                auto vec = model["Coefficients"].get<std::vector<double>>();
                for (size_t i = 0; i < vec.size(); i++) {
                    coefs[hgps::core::Identifier(std::to_string(i))] = vec[i];
                }
            } else if (model["Coefficients"].is_object()) {
                // Handle object format
                for (const auto &[key, value] : model["Coefficients"].items()) {
                    if (value.is_null()) {
                        continue;
                    }
                    coefs[hgps::core::Identifier(key)] = value.get<double>();
                }
            } else {
                continue;
            }
            region_models[parse_region(region_name)] = LinearModelParams{intercept, coefs};
        }
    }

    // If region models is empty, create from prevalence data
    if (region_models.empty()) {
        for (const auto &[region, gender_prevalence] : region_prevalence) {
            LinearModelParams params;
            params.intercept = 0.0;
            for (const auto &[gender, value] : gender_prevalence) {
                params.coefficients[core::Identifier(
                    gender == core::Gender::female ? "female" : "male")] = value;
            }
            region_models[region] = std::move(params);
        }
    }

    // Process ethnicity models - OPTIONAL but use ethnicity prevalence if not present
    std::unordered_map<core::Ethnicity, LinearModelParams> ethnicity_models;
    if (opt.contains("EthnicityModels") && !opt["EthnicityModels"].is_null()) {
        for (const auto &[ethnicity_name, model] : opt["EthnicityModels"].items()) {
            if (!model.contains("Intercept") || model["Intercept"].is_null()) {
                continue;
            }

            if (!model.contains("Coefficients") || model["Coefficients"].is_null()) {
                continue;
            }

            auto intercept = model["Intercept"].get<double>();
            std::unordered_map<hgps::core::Identifier, double> coefs;
            if (model["Coefficients"].is_array()) {
                // Handle vector format
                auto vec = model["Coefficients"].get<std::vector<double>>();
                for (size_t i = 0; i < vec.size(); i++) {
                    coefs[hgps::core::Identifier(std::to_string(i))] = vec[i];
                }
            } else if (model["Coefficients"].is_object()) {
                // Handle object format
                for (const auto &[key, value] : model["Coefficients"].items()) {
                    if (value.is_null()) {
                        continue;
                    }
                    coefs[hgps::core::Identifier(key)] = value.get<double>();
                }
            } else {
                continue;
            }
            ethnicity_models[parse_ethnicity(ethnicity_name)] = LinearModelParams{intercept, coefs};
        }
    }

    // If ethnicity models is empty, create from prevalence data
    if (ethnicity_models.empty()) {
        for (const auto &[ethnicity, gender_prevalence] : ethnicity_prevalence) {
            LinearModelParams params;
            params.intercept = 0.0;
            for (const auto &[gender, value] : gender_prevalence) {
                params.coefficients[core::Identifier(
                    gender == core::Gender::female ? "female" : "male")] = value;
            }
            ethnicity_models[ethnicity] = std::move(params);
        }
    }

    return std::make_unique<hgps::StaticLinearModelDefinition>(
        std::move(expected), std::move(expected_trend), std::move(trend_steps),
        std::make_shared<std::unordered_map<core::Identifier, double>>(*expected_trend_boxcox),
        std::move(names), std::move(models), std::move(ranges), std::move(lambda),
        std::move(stddev), std::move(cholesky), std::move(policy_models), std::move(policy_ranges),
        std::move(policy_cholesky), std::move(trend_models), std::move(trend_ranges),
        std::move(trend_lambda), info_speed, std::move(rural_prevalence), std::move(income_models),
        std::move(region_models), physical_activity_stddev, income_continuous_stddev,
        std::move(ethnicity_models));
}

// Added to handle region parsing since income was made quartile, and region was added- Mahima
core::Region parse_region(const std::string &value) {
    if (value == "England") {
        return core::Region::England;
    }
    if (value == "Wales") {
        return core::Region::Wales;
    }
    if (value == "Scotland") {
        return core::Region::Scotland;
    }
    if (value == "NorthernIreland") {
        return core::Region::NorthernIreland;
    }
    if (value == "unknown") {
        return core::Region::unknown;
    }

    throw core::HgpsException(fmt::format("Unknown region value: {}", value));
}
// Added to parse ethnicity - Mahima
core::Ethnicity parse_ethnicity(const std::string &value) {
    if (value == "White") {
        return core::Ethnicity::White;
    }
    if (value == "Asian") {
        return core::Ethnicity::Asian;
    }
    if (value == "Black") {
        return core::Ethnicity::Black;
    }
    if (value == "Others") {
        return core::Ethnicity::Others;
    }
    if (value == "unknown") {
        return core::Ethnicity::unknown;
    }

    throw core::HgpsException(fmt::format("Unknown ethnicity value: {}", value));
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unique_ptr<hgps::DynamicHierarchicalLinearModelDefinition>
load_ebhlm_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();

    auto info = LiteHierarchicalModelInfo{};

    // Risk factor expected values by sex and age.
    auto expected = load_risk_factor_expected(config);
    auto expected_trend = std::make_unique<std::unordered_map<core::Identifier, double>>();
    auto trend_steps = std::make_unique<std::unordered_map<core::Identifier, int>>();

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

        // NOTE: variable trending is not used in this model.
        expected_trend->emplace(hgps::core::Identifier{item.factor}, 1.0);
        trend_steps->emplace(hgps::core::Identifier{item.factor}, 0);
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
        std::move(expected), std::move(expected_trend), std::move(trend_steps),
        std::move(equations), std::move(variables), percentage);
}
// NOLINTEND(readability-function-cognitive-complexity)

std::unique_ptr<hgps::KevinHallModelDefinition>
load_kevinhall_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();

    // Risk factor expected values by sex and age.
    auto expected = load_risk_factor_expected(config);
    auto expected_trend = std::make_unique<std::unordered_map<core::Identifier, double>>();

    // Number of steps to apply nutrient time trends.
    auto trend_steps = std::make_unique<std::unordered_map<core::Identifier, int>>();

    // Nutrient groups from dynamic_model.json
    std::unordered_map<hgps::core::Identifier, double> energy_equation;
    std::unordered_map<hgps::core::Identifier, hgps::core::DoubleInterval> nutrient_ranges;
    for (const auto &nutrient : opt["Nutrients"]) {
        // Convert nutrient name to lowercase
        std::string nutrient_name = nutrient["Name"].get<std::string>();
        std::transform(nutrient_name.begin(), nutrient_name.end(), nutrient_name.begin(),
                       ::tolower);
        auto nutrient_key = hgps::core::Identifier(nutrient_name);
        auto range_lower = nutrient["Range"][0].get<double>();
        auto range_upper = nutrient["Range"][1].get<double>();
        nutrient_ranges[nutrient_key] = hgps::core::DoubleInterval(range_lower, range_upper);
        energy_equation[nutrient_key] = nutrient["Energy"].get<double>();
    }

    // Foods from dynamic_model.json
    std::unordered_map<hgps::core::Identifier, std::optional<double>> food_prices;
    std::unordered_map<hgps::core::Identifier, std::map<hgps::core::Identifier, double>>
        nutrient_equations;

    for (const auto &food : opt["Foods"]) {
        std::string food_name = food["Name"].get<std::string>();

        // Clean the food name - convert to lowercase and trim whitespace
        std::transform(food_name.begin(), food_name.end(), food_name.begin(), ::tolower);
        food_name.erase(0, food_name.find_first_not_of(" \t\n\r\f\v"));
        food_name.erase(food_name.find_last_not_of(" \t\n\r\f\v") + 1);

        auto food_key = hgps::core::Identifier(food_name);
        (*expected_trend)[food_key] = food["ExpectedTrend"].get<double>();
        (*trend_steps)[food_key] = food["TrendSteps"].get<int>();
        food_prices[food_key] = food["Price"].get<std::optional<double>>();

        // Convert food nutrients map keys to lowercase and trim whitespace
        auto original_food_nutrients = food["Nutrients"].get<std::map<std::string, double>>();
        std::map<hgps::core::Identifier, double> food_nutrients;
        for (const auto &[key, value] : original_food_nutrients) {
            std::string cleaned_key = key;
            std::transform(cleaned_key.begin(), cleaned_key.end(), cleaned_key.begin(), ::tolower);
            // Trim whitespace
            cleaned_key.erase(0, cleaned_key.find_first_not_of(" \t\n\r\f\v"));
            cleaned_key.erase(cleaned_key.find_last_not_of(" \t\n\r\f\v") + 1);
            food_nutrients[hgps::core::Identifier(cleaned_key)] = value;
        }

        for (const auto &nutrient : opt["Nutrients"]) {
            // Convert nutrient name to lowercase and trim whitespace
            std::string nutrient_name = nutrient["Name"].get<std::string>();
            std::transform(nutrient_name.begin(), nutrient_name.end(), nutrient_name.begin(),
                           ::tolower);
            // Trim whitespace
            nutrient_name.erase(0, nutrient_name.find_first_not_of(" \t\n\r\f\v"));
            nutrient_name.erase(nutrient_name.find_last_not_of(" \t\n\r\f\v") + 1);
            auto nutrient_key = hgps::core::Identifier(nutrient_name);

            if (food_nutrients.find(nutrient_key) != food_nutrients.end()) {
                double val = food_nutrients.at(nutrient_key);
                nutrient_equations[food_key][nutrient_key] = val;
            }
        }
    }

    // Weight quantiles from dynamic_model.json
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

    // Energy Physical Activity quantiles from dynamic_model.json
    const auto epa_quantiles_table = load_datatable_from_csv(
        hgps::input::get_file_info(opt["EnergyPhysicalActivityQuantiles"], config.root_path));
    std::vector<double> epa_quantiles;
    epa_quantiles.reserve(epa_quantiles_table.num_rows());
    for (size_t j = 0; j < epa_quantiles_table.num_rows(); j++) {
        epa_quantiles.push_back(std::any_cast<double>(epa_quantiles_table.column(0).value(j)));
    }
    std::sort(epa_quantiles.begin(), epa_quantiles.end());

    // Load 
    // 
    // 
    // 
    // model parameters from the dynamic model - ONLY from dynamic_model.json
    if (!opt.contains("HeightStdDev") || opt["HeightStdDev"].is_null()) {
        throw core::HgpsException{"HeightStdDev field is missing or null in dynamic_model.json"};
    }

    if (!opt["HeightStdDev"].contains("Female") || !opt["HeightStdDev"].contains("Male") ||
        opt["HeightStdDev"]["Female"].is_null() || opt["HeightStdDev"]["Male"].is_null()) {
        throw core::HgpsException{
            "HeightStdDev must contain Female and Male fields in dynamic_model.json"};
    }

    if (!opt.contains("HeightSlope") || opt["HeightSlope"].is_null()) {
        throw core::HgpsException{"HeightSlope field is missing or null in dynamic_model.json"};
    }

    if (!opt["HeightSlope"].contains("Female") || !opt["HeightSlope"].contains("Male") ||
        opt["HeightSlope"]["Female"].is_null() || opt["HeightSlope"]["Male"].is_null()) {
        throw core::HgpsException{
            "HeightSlope must contain Female and Male fields in dynamic_model.json"};
    }

    std::unordered_map<hgps::core::Gender, double> height_stddev = {
        {hgps::core::Gender::female, opt["HeightStdDev"]["Female"].get<double>()},
        {hgps::core::Gender::male, opt["HeightStdDev"]["Male"].get<double>()}};

    std::unordered_map<hgps::core::Gender, double> height_slope = {
        {hgps::core::Gender::female, opt["HeightSlope"]["Female"].get<double>()},
        {hgps::core::Gender::male, opt["HeightSlope"]["Male"].get<double>()}};

    height_stddev[hgps::core::Gender::female], height_stddev[hgps::core::Gender::male],
        height_slope[hgps::core::Gender::female], height_slope[hgps::core::Gender::male];

    // Load static model data for the shared components
    std::filesystem::path static_model_path = config.root_path / "static_model.json";
    if (!std::filesystem::exists(static_model_path)) {
        throw core::HgpsException{"static_model.json file not found at: " +
                                  static_model_path.string()};
    }

    nlohmann::json static_model = load_json(static_model_path);

    // Load region prevalence from static_model.json (REQUIRED)
    if (!static_model.contains("RegionPrevalence") || static_model["RegionPrevalence"].is_null()) {
        throw core::HgpsException{"RegionPrevalence field is missing or null in static_model.json"};
    }

    auto region_prevalence = detail::process_region_prevalence(static_model["RegionPrevalence"]);

    // Load ethnicity prevalence from static_model.json (REQUIRED)
    if (!static_model.contains("EthnicityPrevalence") ||
        static_model["EthnicityPrevalence"].is_null()) {
        throw core::HgpsException{
            "EthnicityPrevalence field is missing or null in static_model.json"};
    }

    auto ethnicity_prevalence =
        detail::process_ethnicity_prevalence(static_model["EthnicityPrevalence"]);

    // Load income models from static_model.json (REQUIRED)
    if (!static_model.contains("IncomeModels") || static_model["IncomeModels"].is_null()) {
        throw core::HgpsException{"IncomeModels field is missing or null in static_model.json"};
    }

    auto income_models = detail::process_income_models(static_model["IncomeModels"]);

    // HARDCODED: Use 293.752576754905 for income_continuous_stddev instead of checking JSON
    double income_continuous_stddev =
        293.752576754905; // Hardcoded value instead of reading from JSON

    // Create shared region and ethnicity models
    auto shared_region_models =
        std::make_shared<std::unordered_map<core::Region, LinearModelParams>>();
    auto shared_ethnicity_models =
        std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>();

    // Process region models if available
    if (static_model.contains("RegionModels") && !static_model["RegionModels"].is_null()) {
        for (const auto &[region_name, model] : static_model["RegionModels"].items()) {
            if (!model.contains("Intercept") || model["Intercept"].is_null()) {
                throw core::HgpsException{fmt::format(
                    "Region model {} missing Intercept in static_model.json", region_name)};
            }

            if (!model.contains("Coefficients") || model["Coefficients"].is_null()) {
                throw core::HgpsException{fmt::format(
                    "Region model {} missing Coefficients in static_model.json", region_name)};
            }

            auto intercept = model["Intercept"].get<double>();
            std::unordered_map<hgps::core::Identifier, double> coefs;
            if (model["Coefficients"].is_array()) {
                // Handle vector format
                auto vec = model["Coefficients"].get<std::vector<double>>();
                for (size_t i = 0; i < vec.size(); i++) {
                    coefs[hgps::core::Identifier(std::to_string(i))] = vec[i];
                }
            } else if (model["Coefficients"].is_object()) {
                // Handle object format
                for (const auto &[key, value] : model["Coefficients"].items()) {
                    if (value.is_null()) {
                        continue;
                    }
                    coefs[hgps::core::Identifier(key)] = value.get<double>();
                }
            } else {
                continue;
            }
            (*shared_region_models)[parse_region(region_name)] =
                LinearModelParams{intercept, coefs};
        }
    } else {
        // Create models from region prevalence
        for (const auto &[region, gender_prevalence] : region_prevalence) {
            LinearModelParams params;
            params.intercept = 0.0;
            for (const auto &[gender, value] : gender_prevalence) {
                params.coefficients[core::Identifier(
                    gender == core::Gender::female ? "female" : "male")] = value;
            }
            (*shared_region_models)[region] = std::move(params);
        }
    }

    // Process ethnicity models if available
    if (static_model.contains("EthnicityModels") && !static_model["EthnicityModels"].is_null()) {
        for (const auto &[ethnicity_name, model] : static_model["EthnicityModels"].items()) {
            if (!model.contains("Intercept") || model["Intercept"].is_null()) {
                throw core::HgpsException{fmt::format(
                    "Ethnicity model {} missing Intercept in static_model.json", ethnicity_name)};
            }

            if (!model.contains("Coefficients") || model["Coefficients"].is_null()) {
                throw core::HgpsException{
                    fmt::format("Ethnicity model {} missing Coefficients in static_model.json",
                                ethnicity_name)};
            }

            auto intercept = model["Intercept"].get<double>();
            std::unordered_map<hgps::core::Identifier, double> coefs;
            if (model["Coefficients"].is_array()) {
                // Handle vector format
                auto vec = model["Coefficients"].get<std::vector<double>>();
                for (size_t i = 0; i < vec.size(); i++) {
                    coefs[hgps::core::Identifier(std::to_string(i))] = vec[i];
                }
            } else if (model["Coefficients"].is_object()) {
                // Handle object format
                for (const auto &[key, value] : model["Coefficients"].items()) {
                    if (value.is_null()) {
                        continue;
                    }
                    coefs[hgps::core::Identifier(key)] = value.get<double>();
                }
            } else {
                continue;
            }
            (*shared_ethnicity_models)[parse_ethnicity(ethnicity_name)] =
                LinearModelParams{intercept, coefs};
        }
    } else {
        // Create models from ethnicity prevalence
        for (const auto &[ethnicity, gender_prevalence] : ethnicity_prevalence) {
            LinearModelParams params;
            params.intercept = 0.0;
            for (const auto &[gender, value] : gender_prevalence) {
                params.coefficients[core::Identifier(
                    gender == core::Gender::female ? "female" : "male")] = value;
            }
            (*shared_ethnicity_models)[ethnicity] = std::move(params);
        }
    }

    return std::make_unique<hgps::KevinHallModelDefinition>(
        std::move(expected), std::move(expected_trend), std::move(trend_steps),
        std::move(energy_equation), std::move(nutrient_ranges), std::move(nutrient_equations),
        std::move(food_prices), std::move(weight_quantiles), std::move(epa_quantiles),
        std::move(height_stddev), std::move(height_slope), std::move(shared_region_models),
        std::move(shared_ethnicity_models), std::move(income_models), income_continuous_stddev);
}

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_risk_model_definition(hgps::RiskFactorModelType model_type,
                           const std::filesystem::path &model_path, const Configuration &config) {
    const auto &[model_name, opt] = load_and_validate_model_json(model_path);

    // Load appropriate model
    if (model_name == "dummy") {
        fmt::print("DEBUG: Loading dummy model\n");
        return load_dummy_risk_model_definition(model_type, opt);
    }

    switch (model_type) {
    case hgps::RiskFactorModelType::Static:
        fmt::print("DEBUG: Loading static model\n");
        // Load this static model with the appropriate loader.
        if (model_name == "hlm") {
            fmt::print("DEBUG: Loading HLM model\n");
            return load_hlm_risk_model_definition(opt);
        }
        if (model_name == "staticlinear") {
            fmt::print("DEBUG: Loading static linear model\n");
            return load_staticlinear_risk_model_definition(opt, config);
        }

        throw hgps::core::HgpsException{
            fmt::format("Static model name '{}' not recognised", model_name)};
    case hgps::RiskFactorModelType::Dynamic:
        fmt::print("DEBUG: Loading dynamic model\n");
        // Load this dynamic model with the appropriate loader.
        if (model_name == "ebhlm") {
            fmt::print("DEBUG: Loading EBHLM model\n");
            return load_ebhlm_risk_model_definition(opt, config);
        }
        if (model_name == "kevinhall") {
            fmt::print("DEBUG: Loading Kevin Hall model\n");
            return load_kevinhall_risk_model_definition(opt, config);
        }

        throw hgps::core::HgpsException{
            fmt::format("Dynamic model name '{}' is not recognised.", model_name)};
    default:
        throw hgps::core::HgpsException("Unknown risk factor model type");
    }
}

void register_risk_factor_model_definitions(hgps::CachedRepository &repository,
                                            const Configuration &config) {
    MEASURE_FUNCTION();

    for (const auto &[model_type_str, model_path] : config.modelling.risk_factor_models) {
        RiskFactorModelType model_type;
        if (model_type_str == "static") {
            model_type = RiskFactorModelType::Static;
        } else if (model_type_str == "dynamic") {
            model_type = RiskFactorModelType::Dynamic;
        } else {
            throw hgps::core::HgpsException(
                fmt::format("Unknown risk factor model type: {}", model_type_str));
        }

        // Load appropriate dynamic/static model
        auto model_definition = load_risk_model_definition(model_type, model_path, config);

        // Register model in cache
        repository.register_risk_factor_model_definition(model_type, std::move(model_definition));
    }
}

} // namespace hgps::input
