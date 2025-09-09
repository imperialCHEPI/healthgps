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
#include <rapidcsv.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>

#if USE_TIMER
#define MEASURE_FUNCTION()
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
// MAHIMA- Dynamic income category mapping from config.json to codebase
/// @brief Maps income category name to enum value based on category count
/// @param key The income category name from JSON
/// @param category_count The number of income categories (3 or 4)
/// @return The corresponding Income enum value
/// @throws core::HgpsException if the category name is unrecognized
hgps::core::Income map_income_category(const std::string &key, const std::string &category_count) {
    if (hgps::core::case_insensitive::equals(key, "unknown")) {
        return hgps::core::Income::unknown;
    }

    if (hgps::core::case_insensitive::equals(key, "low")) {
        return hgps::core::Income::low;
    }

    if (hgps::core::case_insensitive::equals(key, "high")) {
        return hgps::core::Income::high;
    }

    // Handle middle categories based on count
    if (category_count == "3") {
        // 3-category system: low, middle, high
        if (hgps::core::case_insensitive::equals(key, "middle")) {
            return hgps::core::Income::middle;
        }
    } else if (category_count == "4") {
        // 4-category system: low, lowermiddle, uppermiddle, high
        if (hgps::core::case_insensitive::equals(key, "lowermiddle")) {
            return hgps::core::Income::lowermiddle;
        }
        if (hgps::core::case_insensitive::equals(key, "uppermiddle")) {
            return hgps::core::Income::uppermiddle;
        }
    }

    throw hgps::core::HgpsException(fmt::format(
        "Income category '{}' is unrecognized for {} category system", key, category_count));
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

// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unique_ptr<hgps::StaticLinearModelDefinition>
load_staticlinear_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();

    std::cout << "\nDEBUG: Starting load_staticlinear_risk_model_definition";

    // Parse trend_type from config.json- Reda which trend type to use.
    hgps::TrendType trend_type = hgps::TrendType::Null;
    if (config.trend_type == "trend") {
        trend_type = hgps::TrendType::Trend;
    } else if (config.trend_type == "income_trend") {
        trend_type = hgps::TrendType::IncomeTrend;
    }

    // Auto-detect income trend data if present in static_model.json
    // This overrides the config.json setting if income trend data is found
    bool has_income_trend_data = false;
    for (const auto &[key, json_params] : opt["RiskFactorModels"].items()) {
        if (json_params.contains("IncomeTrend")) {
            has_income_trend_data = true;
            break;
        }
    }

    if (has_income_trend_data && trend_type == hgps::TrendType::Null) {
        trend_type = hgps::TrendType::IncomeTrend;
        std::cout << "\nAuto-detected income trend data, setting trend_type to IncomeTrend";
    }

    // Risk factor correlation matrix.
    const auto correlation_file_info =
        input::get_file_info(opt["RiskFactorCorrelationFile"], config.root_path);
    const auto correlation_table = load_datatable_from_csv(correlation_file_info);
    Eigen::MatrixXd correlation{correlation_table.num_rows(), correlation_table.num_columns()};

    // Policy covariance matrix.
    const auto policy_covariance_file_info =
        input::get_file_info(opt["PolicyCovarianceFile"], config.root_path);
    const auto policy_covariance_table = load_datatable_from_csv(policy_covariance_file_info);
    Eigen::MatrixXd policy_covariance{policy_covariance_table.num_rows(),
                                      policy_covariance_table.num_columns()};

    // MAHIMA: Risk factor and intervention policy: names, models, parameters and
    // correlation/covariance.
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

    // MAHIMA: Income trend data structures: income trend models, ranges, lambda, decay factors,
    // steps. These will be nullptr if income trend is not enabled
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend = nullptr;
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox =
        nullptr;
    std::unique_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps = nullptr;
    std::unique_ptr<std::vector<LinearModelParams>> income_trend_models = nullptr;
    std::unique_ptr<std::vector<core::DoubleInterval>> income_trend_ranges = nullptr;
    std::unique_ptr<std::vector<double>> income_trend_lambda = nullptr;
    std::unique_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors =
        nullptr;

    size_t risk_factor_index = 0;
    for (const auto &[key, json_params] : opt["RiskFactorModels"].items()) {
        names.emplace_back(key);

        // Risk factor model parameters.
        LinearModelParams model;
        model.intercept = json_params["Intercept"].get<double>();
        model.coefficients =
            json_params["Coefficients"].get<std::unordered_map<core::Identifier, double>>();

        // Check risk factor correlation matrix column name matches risk factor name.
        auto column_name = correlation_table.column(risk_factor_index).name();
        if (!core::case_insensitive::equals(key, column_name)) {
            throw core::HgpsException{fmt::format("Risk factor {} name ({}) does not match risk "
                                                  "factor correlation matrix column {} name ({})",
                                                  risk_factor_index, key, risk_factor_index,
                                                  column_name)};
        }

        // Write risk factor data structures.
        models.emplace_back(std::move(model));
        ranges.emplace_back(json_params["Range"].get<core::DoubleInterval>());
        lambda.emplace_back(json_params["Lambda"].get<double>());
        stddev.emplace_back(json_params["StdDev"].get<double>());
        for (size_t j = 0; j < correlation_table.num_rows(); j++) {
            correlation(risk_factor_index, j) =
                std::any_cast<double>(correlation_table.column(risk_factor_index).value(j));
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
        auto policy_column_name = policy_covariance_table.column(risk_factor_index).name();
        if (!core::case_insensitive::equals(key, policy_column_name)) {
            throw core::HgpsException{
                fmt::format("Risk factor {} name ({}) does not match intervention "
                            "policy covariance matrix column {} name ({})",
                            risk_factor_index, key, risk_factor_index, policy_column_name)};
        }

        // Write intervention policy data structures.
        policy_models.emplace_back(std::move(policy_model));
        policy_ranges.emplace_back(policy_json_params["Range"].get<core::DoubleInterval>());
        for (size_t j = 0; j < policy_covariance_table.num_rows(); j++) {
            policy_covariance(risk_factor_index, j) =
                std::any_cast<double>(policy_covariance_table.column(risk_factor_index).value(j));
        }

        // MAHIMA: Trend model parameters
        if (trend_type == hgps::TrendType::Null) {
            // No trend data needed for Null type - skip to next risk factor
            std::cout << "\nTrend Type is NULL";
            risk_factor_index++;
            continue;
        }
        if (trend_type == hgps::TrendType::Trend) {
            // Only require trend data if trend type is Trend
            if (json_params.contains("Trend")) {
                // UPF trend data exists - use it
                std::cout << "\nTrend Type is TREND or UPF TREND";
                const auto &trend_json_params = json_params["Trend"];
                LinearModelParams trend_model;
                trend_model.intercept = trend_json_params["Intercept"].get<double>();
                trend_model.coefficients = trend_json_params["Coefficients"]
                                               .get<std::unordered_map<core::Identifier, double>>();
                trend_model.log_coefficients =
                    trend_json_params["LogCoefficients"]
                        .get<std::unordered_map<core::Identifier, double>>();

                // Write real trend data structures.
                trend_models->emplace_back(std::move(trend_model));
                trend_ranges->emplace_back(trend_json_params["Range"].get<core::DoubleInterval>());
                trend_lambda->emplace_back(trend_json_params["Lambda"].get<double>());

                // Load expected value trends (only if trend data exists).
                (*expected_trend)[key] = json_params.contains("ExpectedTrend")
                                             ? json_params["ExpectedTrend"].get<double>()
                                             : 1.0;
                (*expected_trend_boxcox)[key] =
                    json_params.contains("ExpectedTrendBoxCox")
                        ? json_params["ExpectedTrendBoxCox"].get<double>()
                        : 1.0;
                (*trend_steps)[key] =
                    json_params.contains("TrendSteps") ? json_params["TrendSteps"].get<int>() : 0;
            } else {
                throw core::HgpsException{fmt::format(
                    "Trend is enabled but Trend data is missing for risk factor: {}", key)};
            }
        } else {
            // For Null or IncomeTrend types, we don't need regular trend data
            // Add empty trend data structures to maintain consistency
            LinearModelParams trend_model;
            trend_models->emplace_back(std::move(trend_model));
            trend_ranges->emplace_back(core::DoubleInterval{0.0, 1.0});
            trend_lambda->emplace_back(1.0);

            // Set default values for expected trends
            (*expected_trend)[key] = 1.0;
            (*expected_trend_boxcox)[key] = 1.0;
            (*trend_steps)[key] = 0;
        }

        // Income trend model parameters (only if income trend is enabled in the config.json)
        if (trend_type == hgps::TrendType::IncomeTrend) {
            // Create income trend data structures only when income trend is enabled
            std::cout << "\nTrend Type is INCOME TREND";
            if (!expected_income_trend) {
                expected_income_trend =
                    std::make_unique<std::unordered_map<core::Identifier, double>>();
                expected_income_trend_boxcox =
                    std::make_unique<std::unordered_map<core::Identifier, double>>();
                income_trend_steps = std::make_unique<std::unordered_map<core::Identifier, int>>();
                income_trend_models = std::make_unique<std::vector<LinearModelParams>>();
                income_trend_ranges = std::make_unique<std::vector<core::DoubleInterval>>();
                income_trend_lambda = std::make_unique<std::vector<double>>();
                income_trend_decay_factors =
                    std::make_unique<std::unordered_map<core::Identifier, double>>();
            }

            if (json_params.contains("IncomeTrend")) {
                // Read income trend data from static_model.json
                const auto &income_trend_json_params = json_params["IncomeTrend"];
                LinearModelParams income_trend_model;
                income_trend_model.intercept = income_trend_json_params["Intercept"].get<double>();
                income_trend_model.intercept = income_trend_json_params["Intercept"].get<double>();
                income_trend_model.coefficients =
                    income_trend_json_params["Coefficients"]
                        .get<std::unordered_map<core::Identifier, double>>();
                income_trend_model.log_coefficients =
                    income_trend_json_params["LogCoefficients"]
                        .get<std::unordered_map<core::Identifier, double>>();

                // Write real income trend data structures.
                income_trend_models->emplace_back(std::move(income_trend_model));
                income_trend_ranges->emplace_back(
                    income_trend_json_params["Range"].get<core::DoubleInterval>());
                income_trend_lambda->emplace_back(income_trend_json_params["Lambda"].get<double>());

                // Load expected income trend values (no defaults - throw error if missing)
                if (!json_params.contains("ExpectedIncomeTrend")) {
                    throw core::HgpsException{
                        fmt::format("ExpectedIncomeTrend is missing for risk factor: {}", key)};
                }
                if (!json_params.contains("ExpectedIncomeTrendBoxCox")) {
                    throw core::HgpsException{fmt::format(
                        "ExpectedIncomeTrendBoxCox is missing for risk factor: {}", key)};
                }
                if (!json_params.contains("IncomeTrendSteps")) {
                    throw core::HgpsException{
                        fmt::format("IncomeTrendSteps is missing for risk factor: {}", key)};
                }
                if (!json_params.contains("IncomeDecayFactor")) {
                    throw core::HgpsException{
                        fmt::format("IncomeDecayFactor is missing for risk factor: {}", key)};
                }

                (*expected_income_trend)[key] = json_params["ExpectedIncomeTrend"].get<double>();
                (*expected_income_trend_boxcox)[key] =
                    json_params["ExpectedIncomeTrendBoxCox"].get<double>();
                (*income_trend_steps)[key] = json_params["IncomeTrendSteps"].get<int>();
                (*income_trend_decay_factors)[key] = json_params["IncomeDecayFactor"].get<double>();
            } else {
                throw core::HgpsException{fmt::format(
                    "Income trend is enabled but IncomeTrend data is missing for risk factor: {}",
                    key)};
            }
        }

        // Increment table column index.
        risk_factor_index++;
    } // NOLINTEND(readability-function-cognitive-complexity)

    // Check risk factor correlation matrix column count matches risk factor count.
    if (opt["RiskFactorModels"].size() != correlation_table.num_columns()) {
        throw core::HgpsException{fmt::format("Risk factor count ({}) does not match risk "
                                              "factor correlation matrix column count ({})",
                                              opt["RiskFactorModels"].size(),
                                              correlation_table.num_columns())};
    }

    // Compute Cholesky decomposition of the risk factor correlation matrix.
    auto cholesky = Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{correlation}.matrixL()};

    // Check intervention policy covariance matrix column count matches risk factor count.
    if (opt["RiskFactorModels"].size() != policy_covariance_table.num_columns()) {
        throw core::HgpsException{fmt::format("Risk factor count ({}) does not match intervention "
                                              "policy covariance matrix column count ({})",
                                              opt["RiskFactorModels"].size(),
                                              policy_covariance_table.num_columns())};
    }

    // Compute Cholesky decomposition of the intervention policy covariance matrix.
    auto policy_cholesky =
        Eigen::MatrixXd{Eigen::LLT<Eigen::MatrixXd>{policy_covariance}.matrixL()};

    // Risk factor expected values by sex and age.
    auto expected = load_risk_factor_expected(config);

    // Check expected values are defined for all risk factors.
    for (const auto &name : names) {
        if (!expected->at(core::Gender::male).contains(name)) {
            throw core::HgpsException{fmt::format(
                "'{}' is not defined in male risk factor expected values.", name.to_string())};
        }
        if (!expected->at(core::Gender::female).contains(name)) {
            throw core::HgpsException{fmt::format(
                "'{}' is not defined in female risk factor expected values.", name.to_string())};
        }
    }

    // Information speed of risk factor update.
    const double info_speed = opt["InformationSpeed"].get<double>();

    // Rural sector prevalence for age groups and sex.
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>> rural_prevalence;
    for (const auto &age_group : opt["RuralPrevalence"]) {
        auto age_group_name = age_group["Name"].get<core::Identifier>();
        rural_prevalence[age_group_name] = {{core::Gender::female, age_group["Female"]},
                                            {core::Gender::male, age_group["Male"]}};
    }

    // Income models for different income classifications.
    std::unordered_map<core::Income, LinearModelParams> income_models;

    // Read income_categories from config.json to determine which system to use
    std::string income_categories = "3"; // Default to 3-category system
    if (config.config_data.contains("income_categories")) {
        income_categories = config.config_data["income_categories"].get<std::string>();
        // Validate income_categories value
        if (income_categories != "3" && income_categories != "4") {
            throw core::HgpsException{fmt::format(
                "Invalid income_categories: {}. Must be one of: 3, 4", income_categories)};
        }
    }

    // Print which income category system is being used
    std::cout << "Using " << income_categories << " income categories system \n";

    // Check if this is a continuous income model (FINCH approach) or categorical (India approach)
    bool is_continuous_model = false;
    if (opt["IncomeModels"].contains("continuous")) {
        is_continuous_model = true;
        std::cout << "Detected FINCH continuous income model - will calculate continuous income "
                     "then convert to categories\n";

        // For continuous models, we don't need to process IncomeModels here
        // The continuous income calculation will be handled separately
        // We just need to ensure the continuous model exists
        if (!opt["IncomeModels"]["continuous"].contains("Intercept") &&
            !opt["IncomeModels"]["continuous"].contains("csv_file")) {
            throw core::HgpsException("Continuous income model missing required fields: "
                                      "Intercept/Coefficients or csv_file");
        }

        // Create placeholder income models for the categories (these will be filled by the
        // continuous calculation) For now, create empty models - they'll be populated when
        // continuous income is calculated
        income_models.emplace(core::Income::low, LinearModelParams{});
        if (income_categories == "4") {
            income_models.emplace(core::Income::lowermiddle, LinearModelParams{});
            income_models.emplace(core::Income::uppermiddle, LinearModelParams{});
        } else {
            income_models.emplace(core::Income::middle, LinearModelParams{});
        }
        income_models.emplace(core::Income::high, LinearModelParams{});

    } else {
        // This is a categorical income model (India approach)
        std::cout
            << "Detected India categorical income model - directly assigning income categories\n";

        for (const auto &[key, json_params] : opt["IncomeModels"].items()) {
            // Skip if this is a simple/continuous model (already handled above)
            if (key == "simple" || key == "continuous") {
                continue;
            }

            // Get income category using the helper function
            core::Income category = map_income_category(key, income_categories);

            // Get income model parameters.
            LinearModelParams model;
            model.intercept = json_params["Intercept"].get<double>();
            model.coefficients =
                json_params["Coefficients"].get<std::unordered_map<core::Identifier, double>>();

            // Insert income model.
            income_models.emplace(category, std::move(model));
        }
    }

    // MAHIMA: Load physical activity models if present
    // These can be either simple (India approach) or continuous (FINCH approach) or vice versa
    std::unordered_map<core::Identifier, PhysicalActivityModel> physical_activity_models;
    double physical_activity_stddev = 0.0; // Default value

    if (opt.contains("PhysicalActivityModels")) {
        std::cout << "\nLoading PhysicalActivityModels...";

        for (const auto &[model_name, model_config] : opt["PhysicalActivityModels"].items()) {
            PhysicalActivityModel model;
            model.model_type = model_name; // "simple" or "continuous"

            if (model_name == "simple") {
                // India approach: Simple model with standard deviation
                std::cout << "\n  Loading simple model '" << model_name << "' (India approach)";

                if (model_config.contains("PhysicalActivityStdDev")) {
                    model.stddev = model_config["PhysicalActivityStdDev"].get<double>();
                    std::cout << "\n    Standard deviation: " << model.stddev;
                } else {
                    // Use the global PhysicalActivityStdDev as fallback
                    model.stddev = physical_activity_stddev;
                    std::cout << "\n    Using global standard deviation: " << model.stddev;
                }

            } else if (model_name == "continuous") {
                // FINCH approach: Continuous model with CSV file loading
                std::cout << "\n  Loading continuous model '" << model_name << "' (FINCH approach)";

                if (model_config.contains("csv_file")) {
                    std::string csv_filename = model_config["csv_file"].get<std::string>();
                    std::cout << "\n  Loading model '" << model_name
                              << "' from file: " << csv_filename;

                    // For physical activity models, load CSV directly without column mapping
                    // This avoids the column mapping requirement of the CSV parser
                    std::filesystem::path csv_path = config.root_path / csv_filename;

                    // Handle tab delimiter properly
                    std::string delimiter =
                        model_config.contains("delimiter") ? model_config["delimiter"] : ",";
                    if (delimiter == "\\t") {
                        delimiter = "\t"; // Convert escaped tab to actual tab character
                    }

                    // Use rapidcsv directly to load the CSV file (no headers)
                    rapidcsv::Document doc(csv_path.string(), rapidcsv::LabelParams(-1, -1),
                                           rapidcsv::SeparatorParams(delimiter.front()));

                    // Check that we have exactly 2 columns
                    if (doc.GetColumnCount() != 2) {
                        throw core::HgpsException{fmt::format(
                            "Physical activity CSV file {} must have exactly 2 columns. "
                            "Found {} columns",
                            csv_filename, doc.GetColumnCount())};
                    }

                    std::cout << "\n      CSV file loaded: " << doc.GetRowCount() << " rows, "
                              << doc.GetColumnCount() << " columns";
                    std::cout << "\n        Column 0: Factor names";
                    std::cout << "\n        Column 1: Coefficient values";

                    // Parse CSV into PhysicalActivityModel (using existing model variable)
                    // Parse each row (all rows are data, no headers)
                    std::cout << "\n      Parsing CSV data:";
                    for (size_t row_idx = 0; row_idx < doc.GetRowCount(); row_idx++) {
                        // Get factor name and coefficient value directly from rapidcsv
                        std::string factor_name = doc.GetCell<std::string>(0, row_idx);
                        double coefficient_value = doc.GetCell<double>(1, row_idx);

                        std::cout << "\n        Row " << row_idx << ": " << factor_name << " = "
                                  << coefficient_value;

                        if (factor_name == "Intercept") {
                            model.intercept = coefficient_value;
                        } else if (factor_name == "min") {
                            model.min_value = coefficient_value;
                        } else if (factor_name == "max") {
                            model.max_value = coefficient_value;
                        } else if (factor_name == "stddev") {
                            model.stddev = coefficient_value;
                        } else {
                            // All other rows are coefficients
                            model.coefficients[core::Identifier(factor_name)] = coefficient_value;
                        }
                    }

                    std::cout << "\n      Parsed values:";
                    std::cout << "\n        Intercept: " << model.intercept;
                    std::cout << "\n        Coefficients: " << model.coefficients.size();
                    std::cout << "\n        Min: " << model.min_value
                              << ", Max: " << model.max_value;
                    std::cout << "\n        Standard deviation: " << model.stddev;
                } else {
                    throw core::HgpsException{fmt::format(
                        "Continuous physical activity model '{}' must specify 'csv_file'",
                        model_name)};
                }
            } else {
                throw core::HgpsException{fmt::format(
                    "Unknown physical activity model type: '{}'. Must be 'simple' or 'continuous'",
                    model_name)};
            }

            physical_activity_models[core::Identifier(model_name)] = std::move(model);
            std::cout << "\n  DEBUG: Physical activity model '" << model_name
                      << "' stored successfully";
        }

        if (!physical_activity_models.empty()) {
            std::cout << "\n  Successfully loaded " << physical_activity_models.size()
                      << " physical activity models";
        }
    } else {
        std::cout << "\nNo PhysicalActivityModels found, using PhysicalActivityStdDev approach";
        physical_activity_stddev = opt["PhysicalActivityStdDev"].get<double>();
    }

    std::cout << "\nDEBUG: Physical activity models loading completed successfully";
    std::cout << "\nDEBUG: About to create StaticLinearModelDefinition...";

    auto result = std::make_unique<StaticLinearModelDefinition>(
        std::move(expected), std::move(expected_trend), std::move(trend_steps),
        std::move(expected_trend_boxcox), std::move(names), std::move(models), std::move(ranges),
        std::move(lambda), std::move(stddev), std::move(cholesky), std::move(policy_models),
        std::move(policy_ranges), std::move(policy_cholesky), std::move(trend_models),
        std::move(trend_ranges), std::move(trend_lambda), info_speed, std::move(rural_prevalence),
        std::move(income_models), physical_activity_stddev, trend_type,
        std::move(expected_income_trend), std::move(expected_income_trend_boxcox),
        std::move(income_trend_steps), std::move(income_trend_models),
        std::move(income_trend_ranges), std::move(income_trend_lambda),
        std::move(income_trend_decay_factors), is_continuous_model,
        is_continuous_model ? [&opt, &config]() {
            std::cout << "\nDEBUG: Parsing continuous income model...";
            // Parse the continuous income model from CSV file
            LinearModelParams params;
            const auto &continuous_json = opt["IncomeModels"]["continuous"];

            if (continuous_json.contains("csv_file")) {
                std::string csv_filename = continuous_json["csv_file"].get<std::string>();
                std::cout << "\n  Loading continuous income model from file: " << csv_filename;

                // Load CSV file directly using rapidcsv
                std::filesystem::path csv_path = config.root_path / csv_filename;

                // Handle tab delimiter properly
                std::string delimiter = continuous_json.contains("delimiter") ? continuous_json["delimiter"] : ",";
                if (delimiter == "\\t") {
                    delimiter = "\t"; // Convert escaped tab to actual tab character
                }

                // Use rapidcsv directly to load the CSV file (no headers)
                rapidcsv::Document doc(csv_path.string(), rapidcsv::LabelParams(-1, -1),
                                     rapidcsv::SeparatorParams(delimiter.front()));

                // Check that we have exactly 2 columns
                if (doc.GetColumnCount() != 2) {
                    throw core::HgpsException{fmt::format(
                        "Continuous income CSV file {} must have exactly 2 columns. "
                        "Found {} columns",
                        csv_filename, doc.GetColumnCount())};
                }

                std::cout << "\n      CSV file loaded: " << doc.GetRowCount() << " rows, "
                          << doc.GetColumnCount() << " columns";
                std::cout << "\n        Column 0: Factor names";
                std::cout << "\n        Column 1: Coefficient values";

                // Parse CSV into LinearModelParams
                // Parse each row (all rows are data, no headers)
                std::cout << "\n      Parsing CSV data:";
                for (size_t row_idx = 0; row_idx < doc.GetRowCount(); row_idx++) {
                    // Get factor name and coefficient value directly from rapidcsv
                    std::string factor_name = doc.GetCell<std::string>(0, row_idx);
                    double coefficient_value = doc.GetCell<double>(1, row_idx);

                    std::cout << "\n        Row " << row_idx << ": " << factor_name << " = " << coefficient_value;

                    if (factor_name == "Intercept") {
                        params.intercept = coefficient_value;
                    } else {
                        // All other rows are coefficients
                        params.coefficients[core::Identifier(factor_name)] = coefficient_value;
                    }
                }

                std::cout << "\n      Parsed values:";
                std::cout << "\n        Intercept: " << params.intercept;
                std::cout << "\n        Coefficients: " << params.coefficients.size();
                for (const auto &[coef_name, coef_value] : params.coefficients) {
                    std::cout << "\n          " << coef_name.to_string() << ": " << coef_value;
                }
            } else {
                throw core::HgpsException{fmt::format(
                    "Continuous income model must specify 'csv_file'")};
            }

            std::cout << "\nDEBUG: Continuous income model parsing completed";
            return params;
        }() : LinearModelParams{},
        income_categories, std::move(physical_activity_models));

    std::cout << "\nDEBUG: StaticLinearModelDefinition created successfully";
    return result;
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

// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unique_ptr<hgps::KevinHallModelDefinition>
load_kevinhall_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();

    // Risk factor expected values by sex and age.
    auto expected = load_risk_factor_expected(config);
    auto expected_trend = std::make_unique<std::unordered_map<core::Identifier, double>>();

    // Number of steps to apply nutrient time trends.
    auto trend_steps = std::make_unique<std::unordered_map<core::Identifier, int>>();

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

        // Handle ExpectedTrend and TrendSteps with default values for v1 compatibility
        double expected_trend_value = 1.0; // Default for v1 models
        int trend_steps_value = 0;         // Default for v1 models

        if (food.contains("ExpectedTrend")) {
            expected_trend_value = food["ExpectedTrend"].get<double>();
        }
        if (food.contains("TrendSteps")) {
            trend_steps_value = food["TrendSteps"].get<int>();
        }

        (*expected_trend)[food_key] = expected_trend_value;
        (*trend_steps)[food_key] = trend_steps_value;
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
        std::move(expected), std::move(expected_trend), std::move(trend_steps),
        std::move(energy_equation), std::move(nutrient_ranges), std::move(nutrient_equations),
        std::move(food_prices), std::move(weight_quantiles), std::move(epa_quantiles),
        std::move(height_stddev), std::move(height_slope));
}
// NOLINTEND(readability-function-cognitive-complexity)

std::unique_ptr<hgps::RiskFactorModelDefinition>
load_risk_model_definition(hgps::RiskFactorModelType model_type,
                           const std::filesystem::path &model_path, const Configuration &config) {
    const auto &[model_name, opt] = load_and_validate_model_json(model_path);

    // Load appropriate model
    if (model_name == "dummy") {
        return load_dummy_risk_model_definition(model_type, opt);
    }

    switch (model_type) {
    case hgps::RiskFactorModelType::Static:
        // Load this static model with the appropriate loader.
        if (model_name == "hlm") {
            return load_hlm_risk_model_definition(opt);
        }
        if (model_name == "staticlinear") {
            return load_staticlinear_risk_model_definition(opt, config);
        }

        throw hgps::core::HgpsException{
            fmt::format("Static model name '{}' not recognised", model_name)};
    case hgps::RiskFactorModelType::Dynamic:
        // Load this dynamic model with the appropriate loader.
        if (model_name == "ebhlm") {
            return load_ebhlm_risk_model_definition(opt, config);
        }
        if (model_name == "kevinhall") {
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
