#include "model_parser.h"
#include "configuration_parsing_helpers.h"
#include "csvparser.h"
#include "jsonparser.h"
#include "schema.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/scoped_timer.h"
#include "HealthGPS/static_linear_model.h"

#include <Eigen/Cholesky>
#include <Eigen/Dense>
#include <fmt/color.h>
#include <fmt/core.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <rapidcsv.h>

#if USE_TIMER
#define MEASURE_FUNCTION()
hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

namespace {

namespace hc = hgps::core;

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
} // namespace

namespace hgps::input {

// Forward declaration of new function to load risk factor coefficients from CSV- Mahima
// This is better coz then I don't need to use the header file coz the only change is loading from
// CSV instead of JSON
std::unordered_map<std::string, hgps::LinearModelParams>
load_risk_factor_coefficients_from_csv(const std::filesystem::path &csv_path, bool print_debug);

// Forward declaration of new function to load policy ranges from CSV
std::unordered_map<std::string, hgps::core::DoubleInterval>
load_policy_ranges_from_csv(const std::filesystem::path &csv_path);

// Forward declaration of new function to load logistic regression coefficients from CSV
std::unordered_map<std::string, hgps::LinearModelParams>
load_logistic_regression_coefficients_from_csv(const std::filesystem::path &csv_path,
                                               bool print_debug);

// Forward declaration of function to load income model from CSV
std::unordered_map<core::Income, hgps::LinearModelParams>
load_income_model_from_csv(const std::filesystem::path &csv_path);

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
// NOLINTEND(readability-function-cognitive-complexity)
std::unique_ptr<hgps::RiskFactorModelDefinition>
load_staticlinear_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();
    // std::cout << "\nStarting to load Static_model.json";

    // Risk factor correlation matrix.
    const auto correlation_file_info =
        input::get_file_info(opt["RiskFactorCorrelationFile"], config.root_path);
    const auto correlation_table = load_datatable_from_csv(correlation_file_info);
    Eigen::MatrixXd correlation{correlation_table.num_rows(), correlation_table.num_columns()};
    // std::cout << "Finished loading RiskFactorCorrelationFile";

    // Policy covariance matrix.
    const auto policy_covariance_file_info =
        input::get_file_info(opt["PolicyCovarianceFile"], config.root_path);
    const auto policy_covariance_table = load_datatable_from_csv(policy_covariance_file_info);
    Eigen::MatrixXd policy_covariance{policy_covariance_table.num_rows(),
                                      policy_covariance_table.num_columns()};
    // std::cout << "Finished loading PolicyCovarianceFile";

    // Check if boxcox_coefficients.csv for the risk factors boxcox data and the
    // scenario2_food_policyeffect_model.csv for policy data exists in the same directory as the
    // model JSON- Mahima
    // Basically they have to be in the same folder as the JSON files- so in case of FINCH it is
    // C://HealthGPS-examples/KevinHall_FINCH folder
    std::filesystem::path model_dir = config.root_path;
    std::filesystem::path csv_path =
        model_dir / "boxcox_coefficients.csv"; // loading of boxcox for RF
    std::filesystem::path policy_csv_path =
        model_dir / "scenario2_food_policyeffect_model.csv"; // loading of policy for RF
    std::filesystem::path logistic_csv_path =
        model_dir / "logistic_regression.csv"; // loading of logistic regression for 2 stage- risk
                                               // factor modelling
    std::filesystem::path region_csv_path =
        model_dir / "region.csv"; // loading of region for demographic factors
    std::filesystem::path ethnicity_csv_path =
        model_dir / "ethnicity.csv"; // loading of ethnicity for demographic factors
    std::filesystem::path income_csv_path =
        model_dir / "income_model.csv"; // loading of income model parameters
    std::filesystem::path physical_activity_csv_path =
        model_dir / "physicalactivity_model.csv"; // loading of physical activity model parameters

    // Load risk factor coefficients from CSV if the file exists
    std::unordered_map<std::string, hgps::LinearModelParams> csv_coefficients;
    if (std::filesystem::exists(csv_path)) {
        std::cout << "\nFound CSV file for risk factor coefficients: " << csv_path.string() << "\n";
        csv_coefficients = load_risk_factor_coefficients_from_csv(csv_path, false);
    }

    // Load policy coefficients from CSV if the file exists- Mahima
    std::unordered_map<std::string, hgps::LinearModelParams> policy_csv_coefficients;
    std::unordered_map<std::string, hgps::core::DoubleInterval> policy_ranges_map;
    if (std::filesystem::exists(policy_csv_path)) {
        std::cout << "\nFound CSV file for policy coefficients: " << policy_csv_path.string()
                  << "\n";
        policy_csv_coefficients = load_risk_factor_coefficients_from_csv(policy_csv_path, false);
        policy_ranges_map = load_policy_ranges_from_csv(policy_csv_path);
        std::cout << "\nSuccessfully loaded policy coefficients from CSV for "
                  << policy_csv_coefficients.size() << " risk factors";

        // MAHIMA: Print the complete contents of the loaded policy CSV file
        std::cout << "\n======= CONTENTS OF scenario2_food_policyeffect_model.csv =======";
        for (const auto &[risk_factor_name, model_params] : policy_csv_coefficients) {
            std::cout << "\n\nRisk Factor: " << risk_factor_name;
            std::cout << "\n  Intercept: " << model_params.intercept;
            
            // Print all coefficients
            std::cout << "\n  Coefficients:";
            for (const auto &[coef_name, coef_value] : model_params.coefficients) {
                std::cout << "\n    " << coef_name.to_string() << ": " << coef_value;
            }
            
            // Print log coefficients if any
            if (!model_params.log_coefficients.empty()) {
                std::cout << "\n  Log Coefficients:";
                for (const auto &[log_coef_name, log_coef_value] : model_params.log_coefficients) {
                    std::cout << "\n    " << log_coef_name.to_string() << ": " << log_coef_value;
                }
            }
            
            // Print range if available
            if (policy_ranges_map.find(risk_factor_name) != policy_ranges_map.end()) {
                const auto &range = policy_ranges_map[risk_factor_name];
                std::cout << "\n  Range: [" << range.lower() << ", " << range.upper() << "]";
            }
        }
        std::cout << "\n================================================================\n";

        // Print a sample of the loaded policy coefficients for verification
        if (!policy_csv_coefficients.empty()) {
            auto first_rf = policy_csv_coefficients.begin()->first;
            // std::cout << "\n\nSample policy coefficient values for risk factor '" << first_rf <<
            // "':"; std::cout << "\n  Policy Intercept: " <<
            // policy_csv_coefficients[first_rf].intercept;

            // Print a few coefficients
            /*for (const auto &coef_pair : policy_csv_coefficients[first_rf].coefficients) {
                std::cout << "\n  " << coef_pair.first.to_string() << ": " << coef_pair.second;
            }*/

            // Print range if available
            if (policy_ranges_map.find(first_rf) != policy_ranges_map.end()) {
                // std::cout << "\n  Policy range: [" << policy_ranges_map[first_rf].lower() << ",
                // "<< policy_ranges_map[first_rf].upper() << "]";
            }
        }
    }

    // Load logistic regression coefficients from CSV if the file exists
    std::unordered_map<std::string, hgps::LinearModelParams> logistic_coefficients;
    if (std::filesystem::exists(logistic_csv_path)) {
        std::cout << "\nFound CSV file for logistic regression coefficients: "
                  << logistic_csv_path.string() << "\n";
        logistic_coefficients = load_logistic_regression_coefficients_from_csv(logistic_csv_path);
        std::cout << "\nSuccessfully loaded logistic regression coefficients from CSV for "
                  << logistic_coefficients.size() << " risk factors";
    }

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

    size_t i = 0;
    for (const auto &[key, json_params] : opt["RiskFactorModels"].items()) {
        names.emplace_back(key);

        // Risk factor model parameters - check if we have it in CSV first
        LinearModelParams model;

        if (csv_coefficients.find(key) != csv_coefficients.end()) {
            // std::cout << "\nLoading risk factors using .csv YAY!!!";
            //  Use coefficients from CSV file
            model = csv_coefficients[key];

            // Print a sample of the key coefficients for verification
            // std::cout << "\n  Intercept: " << model.intercept;
            /*if (!model.coefficients.empty()) {
                auto first_coef = model.coefficients.begin()->first;
                std::cout << "\n  First coefficient (" << first_coef.to_string()
                          << "): " << model.coefficients[first_coef];
            }*/
        } else {
            // Fall back to JSON if not found in CSV
            std::cout << "\nLoading risk factors using JSON NOOOOOOOOOOO!!!";
            model.intercept = json_params["Intercept"].get<double>();
            model.coefficients =
                json_params["Coefficients"].get<std::unordered_map<core::Identifier, double>>();
        }

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

        // Check if we have policy coefficients in CSV
        if (policy_csv_coefficients.find(key) != policy_csv_coefficients.end()) {
            // Use coefficients from CSV file
            policy_model = policy_csv_coefficients[key];
            // std::cout << "\nLoading policy coefficients using CSV for: " << key;

            // Print a sample of the key coefficients for verification
            // std::cout << "\n  Policy Intercept: " << policy_model.intercept;
            /*if (!policy_model.coefficients.empty()) {
                auto first_coef = policy_model.coefficients.begin()->first;
                std::cout << "\n  First policy coefficient (" << first_coef.to_string() << "): " <<
            policy_model.coefficients[first_coef];
            }*/
        } else {
            // Fall back to JSON if not found in CSV- worst case
            std::cout << "\nLoading policy coefficients using JSON NOOOOOOO!!! for: " << key;
            policy_model.intercept = policy_json_params["Intercept"].get<double>();
            policy_model.coefficients = policy_json_params["Coefficients"]
                                            .get<std::unordered_map<core::Identifier, double>>();
            policy_model.log_coefficients =
                policy_json_params["LogCoefficients"]
                    .get<std::unordered_map<core::Identifier, double>>();
        }

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
        if (policy_csv_coefficients.find(key) != policy_csv_coefficients.end() &&
            policy_ranges_map.find(key) != policy_ranges_map.end()) {
            // Use ranges from CSV
            policy_ranges.emplace_back(policy_ranges_map[key]);
            // std::cout << "\n  Using policy range from CSV: [" << policy_ranges_map[key].lower()
            // << ", " << policy_ranges_map[key].upper() << "]";
        } else {
            // Fall back to JSON ranges
            policy_ranges.emplace_back(policy_json_params["Range"].get<core::DoubleInterval>());
        }
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
        trend_ranges->emplace_back(trend_json_params["Range"].get<core::DoubleInterval>());
        trend_lambda->emplace_back(trend_json_params["Lambda"].get<double>());

        // Load expected value trends.
        (*expected_trend)[key] = json_params["ExpectedTrend"].get<double>();
        (*expected_trend_boxcox)[key] = json_params["ExpectedTrendBoxCox"].get<double>();
        (*trend_steps)[key] = json_params["TrendSteps"].get<int>();

        // Increment table column index.
        i++;
    }

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
    // std::cout << "\nFinished loading Rural Prevelance";

    // Region prevalence for age groups, gender and region.
    // NOLINTBEGIN(readability-function-cognitive-complexity)
    std::unordered_map<core::Identifier,
                       std::unordered_map<core::Gender, std::unordered_map<core::Region, double>>>
        region_prevalence;

    // First load region prevalence from CSV file if it exists
    if (std::filesystem::exists(region_csv_path)) {
        std::cout << "\nLoading region prevalence from CSV file: " << region_csv_path.string()
                  << "\n";
        region_prevalence = load_region_prevalence_from_csv(region_csv_path);

        // Validate that we loaded some data
        if (region_prevalence.empty()) {
            std::cout << "\nWARNING: Failed to load region prevalence from CSV, falling back to "
                         "JSON. NOOOOOO!!!"
                      << "\n";
        } else {
            std::cout << "\nSuccessfully loaded region prevalence from CSV with "
                      << region_prevalence.size() << " age entries" << "\n";
        }
    }

    // If region prevalence is still empty (CSV didn't exist or failed to load), load from JSON
    if (region_prevalence.empty() && opt.contains("RegionPrevalence")) {
        std::cout << "\nLoading region prevalence from JSON configuration" << "\n";
        for (const auto &age_group : opt["RegionPrevalence"]) {
            auto age_group_name = age_group["Name"].get<core::Identifier>();

            // Initialize empty maps for both genders
            std::unordered_map<core::Region, double> female_region_prevalence;
            std::unordered_map<core::Region, double> male_region_prevalence;

            // Parse region prevalence data for each region if available
            if (age_group["Female"].contains("England")) {
                female_region_prevalence[core::Region::England] =
                    age_group["Female"]["England"].get<double>();
                male_region_prevalence[core::Region::England] =
                    age_group["Male"]["England"].get<double>();
            }

            if (age_group["Female"].contains("Wales")) {
                female_region_prevalence[core::Region::Wales] =
                    age_group["Female"]["Wales"].get<double>();
                male_region_prevalence[core::Region::Wales] =
                    age_group["Male"]["Wales"].get<double>();
            }

            if (age_group["Female"].contains("Scotland")) {
                female_region_prevalence[core::Region::Scotland] =
                    age_group["Female"]["Scotland"].get<double>();
                male_region_prevalence[core::Region::Scotland] =
                    age_group["Male"]["Scotland"].get<double>();
            }

            if (age_group["Female"].contains("NorthernIreland")) {
                female_region_prevalence[core::Region::NorthernIreland] =
                    age_group["Female"]["NorthernIreland"].get<double>();
                male_region_prevalence[core::Region::NorthernIreland] =
                    age_group["Male"]["NorthernIreland"].get<double>();
            }

            // Add to the main map
            region_prevalence[age_group_name][core::Gender::female] = female_region_prevalence;
            region_prevalence[age_group_name][core::Gender::male] = male_region_prevalence;

            // Check sum of probabilities
            double female_sum = 0.0;
            double male_sum = 0.0;
            for (const auto &[region, prob] : female_region_prevalence) {
                female_sum += prob;
            }
            for (const auto &[region, prob] : male_region_prevalence) {
                male_sum += prob;
            }
            // std::cout << "\nDEBUG: Sum of probabilities - Female: " << female_sum << ", Male: "
            // << male_sum << std::endl;
        }
        std::cout << "\nLoaded region prevalence from JSON with " << region_prevalence.size()
                  << " age groups" << "\n";
    } else if (region_prevalence.empty()) {
        std::cout << "\nWARNING: No region prevalence data found in CSV or JSON" << "\n";
    }
    // NOLINTEND(readability-function-cognitive-complexity)

    // Ethnicity prevalence for age groups, gender and ethnicity.
    std::unordered_map<
        core::Identifier,
        std::unordered_map<core::Gender, std::unordered_map<core::Ethnicity, double>>>
        ethnicity_prevalence;
    // First load region prevalence from CSV file if it exists
    if (std::filesystem::exists(ethnicity_csv_path)) {
        std::cout << "\nLoading ethnicity prevalence from CSV file: "
                  << ethnicity_csv_path.string();
        ethnicity_prevalence = load_ethnicity_prevalence_from_csv(ethnicity_csv_path);

        // Validate that we loaded some data
        if (ethnicity_prevalence.empty()) {
            std::cout << "\nWARNING: Failed to load ethnicity prevalence from CSV, falling back to "
                         "JSON. NOOOOOO!!!";
        } else {
            std::cout << "\nSuccessfully loaded ethnicity prevalence from CSV with "
                      << ethnicity_prevalence.size() << " age entries" << "\n";
        }
    }
    if (opt.contains("EthnicityPrevalence")) {
        for (const auto &age_group : opt["EthnicityPrevalence"]) {
            auto age_group_name = age_group["Name"].get<core::Identifier>();

            // Process female ethnicity prevalence
            std::unordered_map<core::Ethnicity, double> female_ethnicity_prevalence;
            female_ethnicity_prevalence[core::Ethnicity::White] =
                age_group["Female"]["White"].get<double>();
            female_ethnicity_prevalence[core::Ethnicity::Asian] =
                age_group["Female"]["Asian"].get<double>();
            female_ethnicity_prevalence[core::Ethnicity::Black] =
                age_group["Female"]["Black"].get<double>();
            female_ethnicity_prevalence[core::Ethnicity::Other] =
                age_group["Female"]["Others"].get<double>();

            // Process male ethnicity prevalence
            std::unordered_map<core::Ethnicity, double> male_ethnicity_prevalence;
            male_ethnicity_prevalence[core::Ethnicity::White] =
                age_group["Male"]["White"].get<double>();
            male_ethnicity_prevalence[core::Ethnicity::Asian] =
                age_group["Male"]["Asian"].get<double>();
            male_ethnicity_prevalence[core::Ethnicity::Black] =
                age_group["Male"]["Black"].get<double>();
            male_ethnicity_prevalence[core::Ethnicity::Other] =
                age_group["Male"]["Others"].get<double>();

            // Add to the main map
            ethnicity_prevalence[age_group_name][core::Gender::female] =
                female_ethnicity_prevalence;
            ethnicity_prevalence[age_group_name][core::Gender::male] = male_ethnicity_prevalence;
        }
    }
    // std::cout << "\nFinished loading EthnicityPrevelance";

    // Add detailed debug prints to identify exactly where parsing fails
    // std::cout << "\nDEBUG: About to process IncomeModels";

    // Income models for income_continuous
    std::unordered_map<core::Income, LinearModelParams> income_models;

    // First check if we have a CSV file for income model
    if (std::filesystem::exists(income_csv_path)) {
        std::cout << "\nLoading income model from CSV file: " << income_csv_path.string();
        income_models = load_income_model_from_csv(income_csv_path);
        if (!income_models.empty()) {
            std::cout << "\nSuccessfully loaded income model from CSV";
        }
    }

    // Fall back to JSON if CSV loading failed or didn't exist
    if (income_models.empty() && opt.contains("IncomeModels")) {
        std::cout << "\nLoading income model from JSON NOOOOOOO!!!!";
        for (const auto &[key, json_params] : opt["IncomeModels"].items()) {
            // Get income model parameters
            LinearModelParams model;
            model.intercept = json_params["Intercept"].get<double>();
            model.coefficients =
                json_params["Coefficients"].get<std::unordered_map<core::Identifier, double>>();

            // Convert the string key to core::Income enum
            // For now, we don't distinguish among the key types - we just need one model to work
            // with
            core::Income income_key = core::Income::unknown;

            // Insert income model with the converted enum key
            income_models.emplace(income_key, std::move(model));
        }
    }
    // std::cout << "\nDEBUG: Finished processing IncomeModels";

    // Region models for different region classifications- Mahima
    // Not being used right now as we are using RegionPrevelance

    // std::cout << "\nDEBUG: About to process RegionModels";
    std::unordered_map<core::Region, LinearModelParams> region_models;
    if (opt.contains("RegionModels")) {
        // std::cout << "\nDEBUG: RegionModels entry exists";
        for (const auto &model : opt["RegionModels"]) {
            // std::cout << "\nDEBUG: Processing RegionModel";
            auto region = parse_region(model["Region"].get<std::string>());
            auto params = LinearModelParams{};
            params.intercept = model["Intercept"].get<double>();

            for (const auto &coef : model["Coefficients"]) {
                auto name = coef["Name"].get<core::Identifier>();
                params.coefficients[name] = coef["Value"].get<double>();
            }
            // insert region model with try_emplace to avoid copying the params
            region_models.try_emplace(region, std::move(params));
        }
    } else {
        // std::cout << "\nDEBUG: RegionModels section not found, skipping";
    }
    // std::cout << "\nDEBUG: Finished processing RegionModels";

    // Physical activity models
    std::unordered_map<core::Identifier, LinearModelParams> physical_activity_models;
    double physical_activity_stddev = 0.0;

    // First try to load physical activity models from CSV
    if (std::filesystem::exists(physical_activity_csv_path)) {
        std::cout << "\nFound CSV file for physical activity model: "
                  << physical_activity_csv_path.string();
        try {
            physical_activity_models =
                load_physical_activity_model_from_csv(physical_activity_csv_path);

            // Extract the standard deviation from the model
            if (!physical_activity_models.empty()) {
                auto it = physical_activity_models.begin();
                if (it->second.coefficients.count("stddev") > 0) {
                    physical_activity_stddev = it->second.coefficients.at("stddev");
                    // std::cout << "\nLoaded physical activity standard deviation from CSV: " <<
                    // physical_activity_stddev;
                } else {
                    std::cout << "\nWARNING: No stddev coefficient found in model";
                }

                // Also verify intercept was loaded correctly
                // std::cout << "\nVerified model intercept: " << it->second.intercept;
            }
        } catch (const std::exception &e) {
            std::cout << "\nError loading physical activity model from CSV: " << e.what();
            physical_activity_models.clear(); // Clear any partial data
        }
    } else {
        std::cout << "\nNo physical activity model CSV file found, checking JSON...";
    }

    // Fall back to JSON if needed
    if (physical_activity_models.empty() && opt.contains("PhysicalActivityModels")) {
        std::cout << "\nLoading physical activity models from JSON NOOOOOO!!!!";

        // Validate the structure
        if (!opt["PhysicalActivityModels"].is_object()) {
            std::cout << "\nDEBUG: ERROR - PhysicalActivityModels is not an object, actual type: "
                      << opt["PhysicalActivityModels"].type_name() << "\n";
        } else {
            // std::cout << "\nDEBUG: PhysicalActivityModels has " <<
            // opt["PhysicalActivityModels"].size() << " entries" << std::endl;

            // Process each model
            for (const auto &[key, json_params] : opt["PhysicalActivityModels"].items()) {
                LinearModelParams model;

                // Get the intercept
                if (json_params.contains("Intercept")) {
                    model.intercept = json_params["Intercept"].get<double>();
                    // std::cout << "\nDEBUG: Loaded intercept: " << model.intercept;
                } else {
                    std::cout << "\nWARNING - Missing Intercept for model " << key;
                    model.intercept = 0.0;
                }

                // Get the coefficients
                if (json_params.contains("Coefficients")) {
                    auto coeffs = std::unordered_map<core::Identifier, double>();

                    // Check if Coefficients is an object
                    if (json_params["Coefficients"].is_object()) {
                        for (const auto &[coeff_key, coeff_value] :
                             json_params["Coefficients"].items()) {
                            if (coeff_value.is_number()) {
                                double value = coeff_value.get<double>();
                                coeffs[core::Identifier(coeff_key)] = value;
                                // std::cout << "\nDEBUG: Loaded coefficient " << coeff_key << " = "
                                // << value;
                            } else {
                                std::cout << "\nWARNING - Coefficient " << coeff_key
                                          << " is not a number, skipping";
                            }
                        }
                    } else {
                        std::cout << "\nERROR - Coefficients is not an object, actual type: "
                                  << json_params["Coefficients"].type_name();
                    }

                    model.coefficients = std::move(coeffs);
                    // std::cout << "\nDEBUG: Loaded " << model.coefficients.size() <<
                    // coefficients";
                } else {
                    std::cout << "\nWARNING - No Coefficients section found for model " << key;
                }

                // Handle StandardDeviation if present at this level
                if (json_params.contains("StandardDeviation")) {
                    double pa_stddev = json_params["StandardDeviation"].get<double>();
                    model.coefficients[core::Identifier("StandardDeviation")] = pa_stddev;
                    physical_activity_stddev = pa_stddev;
                }

                // Store the model
                core::Identifier model_key(key);
                physical_activity_models.emplace(model_key, model);
            }
        }
    }

    // If neither CSV nor JSON had physical activity models
    if (physical_activity_models.empty()) {
        std::cout << "\nNo physical activity models found in CSV or JSON";
    }

    // Fallback for standard deviation if not found in models
    if (physical_activity_stddev == 0.0 && physical_activity_models.empty()) {
        if (opt.contains("PhysicalActivityStdDev")) {
            physical_activity_stddev = opt["PhysicalActivityStdDev"].get<double>();
            std::cout << "\nUsing PhysicalActivityStdDev from JSON: " << physical_activity_stddev;
        } else {
            std::cout
                << "\nWARNING: No physical activity standard deviation found in models or JSON";
        }
    }

    // std::cout << "\nDEBUG: About to create StaticLinearModelDefinition";
    // NOLINTBEGIN(readability-function-cognitive-complexity)
    return std::make_unique<StaticLinearModelDefinition>(
        std::move(expected), std::move(expected_trend), std::move(trend_steps),
        std::move(expected_trend_boxcox), std::move(names), std::move(models), std::move(ranges),
        std::move(lambda), std::move(stddev), std::move(cholesky), std::move(policy_models),
        std::move(policy_ranges), std::move(policy_cholesky), std::move(trend_models),
        std::move(trend_ranges), std::move(trend_lambda), info_speed, std::move(rural_prevalence),
        std::move(region_prevalence), std::move(ethnicity_prevalence), std::move(income_models),
        std::move(region_models), physical_activity_stddev, physical_activity_models,
        [&]() -> std::vector<LinearModelParams> {
            if (!logistic_coefficients.empty()) {
                // Convert unordered_map to vector in the same order as names
                std::vector<LinearModelParams> logistic_models_vec;
                for (const auto &name : names) {
                    // Convert name to lowercase for case-insensitive comparison
                    std::string name_str = name.to_string();
                    std::string name_lower = core::to_lower(name_str);

                    // Look for either the original name or a case-insensitive match
                    bool found = false;
                    for (const auto &[key, model] : logistic_coefficients) {
                        if (core::to_lower(key) == name_lower) {
                            logistic_models_vec.push_back(model);
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        // MAHIMA: Changed approach - if no logistic coefficients for this risk
                        // factor, create an empty model instead of using boxcox coefficients as
                        // fallback. Missing logistic data is intentional and means skip Stage 1
                        // (logistic regression) and go directly to Stage 2 (boxcox transformation).
                        std::cout << "\nINFO: No logistic regression coefficients found for "
                                  << name.to_string()
                                  << ", will use boxcox-only modeling (skip Stage 1)";

                        // Create empty model to signal: skip logistic stage, use boxcox only
                        logistic_models_vec.push_back(LinearModelParams{});
                    }
                }
                return logistic_models_vec;
            } else {
                // If no logistic coefficients, return empty vector (StaticLinearModelDefinition
                // will handle this)
                return std::vector<LinearModelParams>{};
            }
        }());
}
// NOLINTEND(readability-function-cognitive-complexity)

// Added to handle region parsing since income was made quartile, and region was added
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

// NONLINTBEGIN(readability-function-cognitive-complexity)
std::unique_ptr<hgps::KevinHallModelDefinition>
load_kevinhall_risk_model_definition(const nlohmann::json &opt, const Configuration &config) {
    MEASURE_FUNCTION();
    // std::cout << "\nStarted loading Kevin Hall risk values";

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

    // Add additional important ranges from config.json risk_factors section- Mahima
    try {
        // Load config.json to get additional ranges
        auto config_json = load_json(config.root_path / "config.json");
        if (config_json.contains("modelling") &&
            config_json["modelling"].contains("risk_factors")) {

            // List of important risk factors to load ranges for
            const std::vector<std::string> important_factors = {
                "Weight", "Height", "BMI", "EnergyIntake", "Age", "Age2", "Age3"};

            int factors_added = 0;
            for (const auto &risk_factor : config_json["modelling"]["risk_factors"]) {
                const std::string rf_name = risk_factor["name"];

                // Check if this is one of our important factors
                if (std::find(important_factors.begin(), important_factors.end(), rf_name) !=
                    important_factors.end()) {
                    hgps::core::Identifier factor_key(rf_name);
                    nutrient_ranges[factor_key] =
                        risk_factor["range"].get<hgps::core::DoubleInterval>();
                    factors_added++;

                    std::cout << "\nAdded " << rf_name << " range from config.json: ["
                              << nutrient_ranges[factor_key].lower() << ", "
                              << nutrient_ranges[factor_key].upper() << "]";
                }
            }

            std::cout << "\nLoaded " << factors_added
                      << " additional factor ranges from config.json";
        }
    } catch (const std::exception &e) {
        std::cout << "\nWARNING: Failed to load additional ranges from config.json: " << e.what();
    }

    // std::cout << "\nFinished loading Kevin Hall nutrients";

    // Food groups.
    std::unordered_map<hgps::core::Identifier, std::map<hgps::core::Identifier, double>>
        nutrient_equations;
    std::unordered_map<hgps::core::Identifier, std::optional<double>> food_prices;
    for (const auto &food : opt["Foods"]) {
        auto food_key = food["Name"].get<hgps::core::Identifier>();
        (*expected_trend)[food_key] = food["ExpectedTrend"].get<double>();
        (*trend_steps)[food_key] = food["TrendSteps"].get<int>();
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
    // std::cout << "\nFinished loading KevinHall Foods";

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
    std::cout << "\nFINISHED ALL THE LOADING REQUIRED CUTIEPIE :)\n";
}

// Function to load risk factor coefficients for boxcox from a CSV file- Mahima
// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unordered_map<std::string, hgps::LinearModelParams>
load_risk_factor_coefficients_from_csv(const std::filesystem::path &csv_path, bool print_debug) {
    MEASURE_FUNCTION();

    // Map to store the result of coefficient loading from CSV
    std::unordered_map<std::string, hgps::LinearModelParams> result;

    // Check if the file exists
    if (!std::filesystem::exists(csv_path)) {
        std::cout << "\nWARNING: CSV file for risk factor coefficients not found: "
                  << csv_path.string() << "\n";
        return result;
    }

    try {
        // Use rapidcsv to read the CSV file
        rapidcsv::Document doc(csv_path.string());

        // Get column names from the header (risk factor names)
        auto risk_factor_names = doc.GetColumnNames();

        // Skip the first column which contains row identifiers
        if (risk_factor_names.size() > 0) {
            risk_factor_names.erase(risk_factor_names.begin());
        }

        // Mapping from CSV row names to internal coefficient names
        std::unordered_map<std::string, std::string> row_name_mapping = {
            {"Intercept", "Intercept"},
            {"gender2", "Gender"}, // gender2 = Female (Male is reference category 0)
            {"age1", "Age"},
            {"age2", "Age2"},
            {"region2", "Wales"},
            {"region3", "Scotland"},
            {"region4", "NorthernIreland"},
            {"ethnicity2", "Asian"},
            {"ethnicity3", "Black"},
            {"ethnicity4", "Others"},
            {"income", "income_continuous"},
            {"EnergyIntake", "EnergyIntake"}};

        // Get all row names (coefficient types)
        auto row_count = doc.GetRowCount();
        std::vector<std::string> row_names;
        for (size_t i = 0; i < row_count; i++) {
            // Get the first column value which is the row name
            row_names.push_back(doc.GetCell<std::string>(0, i));
        }

        // Initialize a LinearModelParams object for each risk factor
        for (const auto &rf_name : risk_factor_names) {
            result[rf_name] = hgps::LinearModelParams();
        }

        // Read values for each risk factor and coefficient
        for (size_t row_idx = 0; row_idx < row_count; row_idx++) {
            std::string row_name = row_names[row_idx];

            // Skip min/max rows as they're for ranges
            if (row_name == "min" || row_name == "max") {
                continue;
            }

            // Map the row name to the internal coefficient name
            std::string coef_name = row_name;
            if (row_name_mapping.find(row_name) != row_name_mapping.end()) {
                coef_name = row_name_mapping[row_name];
            }

            // For each risk factor column
            for (size_t col_idx = 0; col_idx < risk_factor_names.size(); col_idx++) {
                std::string rf_name = risk_factor_names[col_idx];

                // Read the value from the CSV
                double value = doc.GetCell<double>(col_idx + 1,
                                                   row_idx); // +1 because first column is row names

                // Set the appropriate value based on the row type
                if (coef_name == "Intercept") {
                    result[rf_name].intercept = value;
                } else {
                    // Add to coefficients map using the mapped coefficient name
                    result[rf_name].coefficients[hgps::core::Identifier(coef_name)] = value;
                }
            }
        }

        if (print_debug) {
            std::cout << "\nSuccessfully loaded risk factor coefficients from CSV: \n"
                      << csv_path.string();

            // Print a sample of the loaded coefficient values for verification
            if (!result.empty()) {
                // Take the first risk factor as an example
                auto first_rf = result.begin()->first;
                // std::cout << "\n\nSample coefficient values for risk factor '" << first_rf <<
                // "':"; std::cout << "\n  Intercept: " << result[first_rf].intercept;

                // Print a few coefficients
                for (const auto &coef_pair : result[first_rf].coefficients) {
                    std::cout << "\n  " << coef_pair.first.to_string() << ": " << coef_pair.second;
                }

                // Print total number of risk factors and coefficients loaded
                // std::cout << "\n\nTotal risk factors loaded: " << result.size();
                // std::cout << "\nAverage coefficients per risk factor: " << (result.empty() ? 0 :
                // result.begin()->second.coefficients.size());
            }
        }
    } catch (const std::exception &e) {
        std::cout << "\nERROR: Failed to load risk factor coefficients from CSV: " << e.what();
    }

    return result;
}
// NOLINTEND(readability-function-cognitive-complexity)

// Function to load policy ranges from CSV- Mahima
// could have reused the boxcox loading code but anyways did separate function for safety and maybe
// future upgrades
// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unordered_map<std::string, hgps::core::DoubleInterval>
load_policy_ranges_from_csv(const std::filesystem::path &csv_path) {
    MEASURE_FUNCTION();

    // Map to store the result of range loading from CSV
    std::unordered_map<std::string, hgps::core::DoubleInterval> result;
    // Temporary storage for min and max values
    std::unordered_map<std::string, double> min_values;
    std::unordered_map<std::string, double> max_values;

    // Check if the file exists
    if (!std::filesystem::exists(csv_path)) {
        std::cout << "\nWARNING: CSV file for policy ranges not found: " << csv_path.string()
                  << "\n";
        return result;
    }

    try {
        // Use rapidcsv to read the CSV file
        rapidcsv::Document doc(csv_path.string());

        // Get column names from the header (risk factor names)
        auto risk_factor_names = doc.GetColumnNames();

        // Skip the first column which contains row identifiers
        if (risk_factor_names.size() > 0) {
            risk_factor_names.erase(risk_factor_names.begin());
        }

        // Get all row names
        auto row_count = doc.GetRowCount();
        std::vector<std::string> row_names;
        for (size_t i = 0; i < row_count; i++) {
            row_names.push_back(doc.GetCell<std::string>(0, i));
        }

        // Read min/max values first
        for (size_t row_idx = 0; row_idx < row_count; row_idx++) {
            std::string row_name = row_names[row_idx];

            if (row_name == "min" || row_name == "max") {
                for (size_t col_idx = 0; col_idx < risk_factor_names.size(); col_idx++) {
                    std::string rf_name = risk_factor_names[col_idx];
                    double value = doc.GetCell<double>(col_idx + 1, row_idx);

                    if (row_name == "min") {
                        min_values[rf_name] = value;
                    } else {
                        max_values[rf_name] = value;
                    }
                }
            }
        }

        // Now create the intervals correctly- Mahima
        // peviosuly I had an error where it was swapping the min and max coz of the interval.h
        // functionality
        for (const auto &rf_name : risk_factor_names) {
            // Use the values directly without validation
            // This ensures we take exactly what's in the CSV
            double min_val = 0.0;
            double max_val = 0.0;

            if (min_values.find(rf_name) != min_values.end()) {
                min_val = min_values[rf_name];
            }

            if (max_values.find(rf_name) != max_values.end()) {
                max_val = max_values[rf_name];
            }

            // If min > max, swap them to prevent exceptions
            if (min_val > max_val) {
                std::cout << "\nWARNING: For " << rf_name << ", min (" << min_val << ") > max ("
                          << max_val << "). Swapping values.";
                std::swap(min_val, max_val);
            }

            // Create interval with correct values
            result[rf_name] = hgps::core::DoubleInterval(min_val, max_val);
        }

        std::cout << "\nSuccessfully loaded policy ranges from CSV";

        // Print a sample of the loaded ranges for verification
        if (!result.empty()) {
            // Take the first risk factor as an example
            auto first_rf = result.begin()->first;
            // std::cout << "\n\nSample range values for policy risk factor '" << first_rf << "':";
            // std::cout << "\n  Range: [" << result[first_rf].lower() << ", "<<
            // result[first_rf].upper() << "]";
            std::cout << "\n\nTotal policy ranges loaded: " << result.size() << "\n";
        }
    } catch (const std::exception &e) {
        std::cout << "\nERROR: Failed to load policy ranges from CSV: " << e.what();
    }

    return result;
}
// NOLINTEND(readability-function-cognitive-complexity)

// Function for the implementation of load_logistic_regression_coefficients_from_csv- Mahima
// I can reuse the boxcox laoding function but for safety, I wrote another. We can change this
// later.
// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unordered_map<std::string, hgps::LinearModelParams>
load_logistic_regression_coefficients_from_csv(const std::filesystem::path &csv_path,
                                               bool print_debug) {
    MEASURE_FUNCTION();

    // Map to store the result of logistic regression coefficient loading from CSV
    std::unordered_map<std::string, hgps::LinearModelParams> result;

    // Check if the file exists
    if (!std::filesystem::exists(csv_path)) {
        std::cout << "\nWARNING: CSV file for logistic regression coefficients not found: "
                  << csv_path.string() << "\n";
        return result;
    }

    try {
        // Use rapidcsv to read the CSV file
        rapidcsv::Document doc(csv_path.string());

        // Get column names from the header (risk factor names)
        auto risk_factor_names = doc.GetColumnNames();

        // Skip the first column which contains row identifiers
        if (risk_factor_names.size() > 0) {
            risk_factor_names.erase(risk_factor_names.begin());
        }

        // Mapping from CSV row names to internal coefficient names
        std::unordered_map<std::string, std::string> row_name_mapping = {
            {"Intercept", "Intercept"},
            {"gender2", "Gender"}, // gender2 = Female (Male is reference category 0)
            {"age1", "Age"},
            {"age2", "Age2"},
            {"region2", "Wales"},
            {"region3", "Scotland"},
            {"region4", "NorthernIreland"},
            {"ethnicity2", "Asian"},
            {"ethnicity3", "Black"},
            {"ethnicity4", "Others"},
            {"income", "income_continuous"},
            {"EnergyIntake", "EnergyIntake"}};

        // Get all row names (coefficient types)
        auto row_count = doc.GetRowCount();
        std::vector<std::string> row_names;
        for (size_t i = 0; i < row_count; i++) {
            // Get the first column value which is the row name
            row_names.push_back(doc.GetCell<std::string>(0, i));
        }

        // Initialize a LinearModelParams object for each risk factor
        for (const auto &rf_name : risk_factor_names) {
            result[rf_name] = hgps::LinearModelParams();
        }

        // Read values for each risk factor and coefficient
        for (size_t row_idx = 0; row_idx < row_count; row_idx++) {
            std::string row_name = row_names[row_idx];

            // Skip min/max rows as they're for ranges
            if (row_name == "min" || row_name == "max") {
                continue;
            }

            // Map the row name to the internal coefficient name
            std::string coef_name = row_name;
            if (row_name_mapping.find(row_name) != row_name_mapping.end()) {
                coef_name = row_name_mapping[row_name];
            }

            // For each risk factor column
            for (size_t col_idx = 0; col_idx < risk_factor_names.size(); col_idx++) {
                std::string rf_name = risk_factor_names[col_idx];

                // Read the value from the CSV
                double value = doc.GetCell<double>(col_idx + 1,
                                                   row_idx); // +1 because first column is row names

                // Set the appropriate value based on the row type
                if (coef_name == "Intercept") {
                    result[rf_name].intercept = value;
                } else {
                    // Add to coefficients map using the mapped coefficient name
                    result[rf_name].coefficients[hgps::core::Identifier(coef_name)] = value;
                }
            }
        }

        if (print_debug) {
            std::cout << "\nSuccessfully loaded logistic regression coefficients from CSV: \n"
                      << csv_path.string();

            // Print a sample of the loaded coefficient values for verification
            if (!result.empty()) {
                // Take the first risk factor as an example
                auto first_rf = result.begin()->first;
                // std::cout << "\n\nSample logistic regression coefficient values for risk factor
                // '" << first_rf << "':"; std::cout << "\n  Intercept: " <<
                // result[first_rf].intercept;

                // Print a few coefficients
                for (const auto &coef_pair : result[first_rf].coefficients) {
                    std::cout << "\n  " << coef_pair.first.to_string() << ": " << coef_pair.second;
                }

                // Print total number of risk factors and coefficients loaded
                std::cout << "\n\nTotal risk factors with logistic regression coefficients loaded: "
                          << result.size();
                // std::cout << "\nAverage coefficients per risk factor: " << (result.empty() ? 0 :
                // result.begin()->second.coefficients.size());
            }
        }
    } catch (const std::exception &e) {
        std::cout << "\nERROR: Failed to load logistic regression coefficients from CSV: "
                  << e.what();
    }

    return result;
}
// NOLINTEND(readability-function-cognitive-complexity)

// Function to load the region data using csv- Mahima
// For code maintainability, I'm using different functions for each as region, ethnicity, income and
// physical acivity are being used to assign demographics to people and not like risk factors where
// we don't have particularly verify what each column is
// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unordered_map<core::Identifier,
                   std::unordered_map<core::Gender, std::unordered_map<core::Region, double>>>
load_region_prevalence_from_csv(const std::filesystem::path &csv_path) {
    MEASURE_FUNCTION();

    // Map to store the result of region prevalence loading from CSV
    std::unordered_map<core::Identifier,
                       std::unordered_map<core::Gender, std::unordered_map<core::Region, double>>>
        result;

    // Check if the file exists in the same directory as the JSON input files
    if (!std::filesystem::exists(csv_path)) {
        std::cout << "\nWARNING: CSV file for region prevalence not found: " << csv_path.string()
                  << "\n";
        return result;
    }

    try {
        // Use rapidcsv to read the CSV file
        rapidcsv::Document doc(csv_path.string());

        // Process each row in the CSV file
        // Removed unused variable: int sample_count = 0;
        // std::cout << "\n--- SAMPLE OF REGION DATA LOADED FROM CSV ---" ;
        // std::cout << "Age | Gender | England | Wales | Scotland | N.Ireland" ;

        for (size_t i = 0; i < doc.GetRowCount(); i++) {
            // Get all values for this row
            auto row = doc.GetRow<std::string>(i);

            // Expected columns: Age, Gender, England, Wales, Scotland, Northern Ireland
            if (row.size() < 6) {
                std::cout
                    << "\nWARNING: Invalid row format in region prevalence CSV, skipping row: " << i
                    << "\n";
                continue;
            }

            // Parse age
            int age = std::stoi(row[0]);
            // Create an age identifier in the format "age_X" (not starting with a number)
            core::Identifier age_id("age_" + std::to_string(age));

            // Parse gender (0 = male, 1 = female) and map to correct values
            core::Gender gender =
                (std::stoi(row[1]) == 0) ? core::Gender::male : core::Gender::female;

            // Parse region probabilities
            double england_prob = std::stod(row[2]);
            double wales_prob = std::stod(row[3]);
            double scotland_prob = std::stod(row[4]);
            double ni_prob = std::stod(row[5]);

            // Print a sample of the first few rows
            /*if (sample_count < 5) {
                std::cout << age << " | " << (gender == core::Gender::male ? "male" : "female")
                          << " | " << england_prob << " | " << wales_prob << " | " << scotland_prob
                          << " | " << ni_prob << std::endl;
                sample_count++;
            }*/

            // Store in result map with same structure as the existing JSON-loaded data
            result[age_id][gender][core::Region::England] = england_prob;
            result[age_id][gender][core::Region::Wales] = wales_prob;
            result[age_id][gender][core::Region::Scotland] = scotland_prob;
            result[age_id][gender][core::Region::NorthernIreland] = ni_prob;

            // Validate probabilities sum to approximately 1.0
            double sum = england_prob + wales_prob + scotland_prob + ni_prob;
            if (std::abs(sum - 1.0) > 0.001) {
                std::cout << "\nWARNING: Region probabilities for age " << age << ", gender "
                          << (gender == core::Gender::male ? "male" : "female") << " sum to " << sum
                          << " (should be 1.0)" << "\n";
            }
        }

        // std::cout << "\n--- END OF SAMPLE ---" << std::endl;
        std::cout << "\nLoaded region prevalence data for " << result.size()
                  << " age groups from CSV file" << "\n";
    } catch (const std::exception &e) {
        std::cout << "\nERROR: Failed to process region prevalence CSV: " << e.what() << "\n";
    }

    return result;
}
// NOLINTEND(readability-function-cognitive-complexity)

// Function to load the region data using csv- Mahima
// For code maintainability, I'm using different functions for each as region, ethnicity, income and
// physical acivity are being used to assign demographics to people and not like risk factors where
// we don't have particularly verify what each column is
// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unordered_map<core::Identifier,
                   std::unordered_map<core::Gender, std::unordered_map<core::Ethnicity, double>>>
load_ethnicity_prevalence_from_csv(const std::filesystem::path &csv_path) {
    MEASURE_FUNCTION();

    // Map to store the result of ethnicity prevalence loading from CSV
    std::unordered_map<
        core::Identifier,
        std::unordered_map<core::Gender, std::unordered_map<core::Ethnicity, double>>>
        result;

    // Check if the file exists in the same directory as the JSON input files
    if (!std::filesystem::exists(csv_path)) {
        std::cout << "\nWARNING: CSV file for ethnicity prevalence not found: " << csv_path.string()
                  << "\n";
        return result;
    }

    try {
        // Use rapidcsv to read the CSV file
        rapidcsv::Document doc(csv_path.string());

        // Process each row in the CSV file
        // int sample_count = 0;
        // std::cout << "\n--- SAMPLE OF ETHNICITY DATA LOADED FROM CSV ---" << std::endl;
        // std::cout << "Adult | Gender | Ethnicity | England | Wales | N.Ireland | Scotland";

        for (size_t i = 0; i < doc.GetRowCount(); i++) {
            // Get all values for this row
            auto row = doc.GetRow<std::string>(i);

            // Expected columns: adult, gender, ethnicity, England, Wales, Northern Ireland,
            // Scotland
            if (row.size() < 7) {
                std::cout
                    << "\nWARNING: Invalid row format in ethnicity prevalence CSV, skipping row: "
                    << i << "\n";
                continue;
            }

            // Parse adult/age group (0 = Under18, 1 = Over18)
            int adult = std::stoi(row[0]);
            // Create an age group identifier (Under18/Over18)
            core::Identifier age_id = (adult == 0) ? "Under18"_id : "Over18"_id;

            // Parse gender (0 = male, 1 = female) and map to correct values
            core::Gender gender =
                (std::stoi(row[1]) == 0) ? core::Gender::male : core::Gender::female;

            // Parse ethnicity type (1 = White, 2 = Asian, 3 = Black, 4 = Other)
            int ethnicity_code = std::stoi(row[2]);
            core::Ethnicity ethnicity;
            switch (ethnicity_code) {
            case 1:
                ethnicity = core::Ethnicity::White;
                break;
            case 2:
                ethnicity = core::Ethnicity::Asian;
                break;
            case 3:
                ethnicity = core::Ethnicity::Black;
                break;
            case 4:
                ethnicity = core::Ethnicity::Other;
                break;
            default:
                std::cout << "\nWARNING: Unknown ethnicity code: " << ethnicity_code
                          << ", skipping row " << i << "\n";
                continue;
            }

            // Get England probability for this specific ethnicity
            // (we only use England probabilities as specified)
            double england_prob = std::stod(row[3]);

            // These variables are unused - removed to fix compiler warnings
            // double wales_prob = std::stod(row[4]);
            // double ni_prob = std::stod(row[5]);
            // double scotland_prob = std::stod(row[6]);

            // Print a sample of the first few rows
            /*if (sample_count < 5) {
                std::cout << adult << " | " << (gender == core::Gender::male ? "male" : "female")
                          << " | " << ethnicity_code << " | " << england_prob << " | " << wales_prob
                          << " | " << ni_prob << " | " << scotland_prob << std::endl;
                sample_count++;
            }*/

            // Store in result map with same structure as the existing JSON-loaded data
            // Each row in the CSV represents one ethnicity for a specific age group and gender
            result[age_id][gender][ethnicity] = england_prob;
        }

        // Check and normalize probabilities for each age group and gender
        for (auto &[age_id, gender_map] : result) {
            for (auto &[gender, ethnicity_map] : gender_map) {
                // Calculate the sum of all probabilities
                double sum = 0.0;
                for (const auto &[ethnicity, prob] : ethnicity_map) {
                    sum += prob;
                }

                // Normalize if sum is not close to 1.0
                if (std::abs(sum - 1.0) > 0.001) {
                    std::cout << "\nNormalizing ethnicity probabilities for " << age_id.to_string()
                              << ", gender " << (gender == core::Gender::male ? "male" : "female")
                              << " (sum was " << sum << ")" << "\n";

                    // Normalize each probability
                    for (auto &[ethnicity, prob] : ethnicity_map) {
                        prob /= sum;
                    }
                }
            }
        }

        // std::cout << "\n--- END OF SAMPLE ---" << std::endl;
        std::cout << "\nLoaded ethnicity prevalence data for " << result.size()
                  << " age groups from CSV file" << "\n";
    } catch (const std::exception &e) {
        std::cout << "\nERROR: Failed to process ethnicity prevalence CSV: " << e.what() << "\n";
    }

    return result;
}
// NOLINTEND(readability-function-cognitive-complexity)

// Function to load income model using csv- Mahima
// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unordered_map<core::Income, hgps::LinearModelParams>
load_income_model_from_csv(const std::filesystem::path &csv_path) {
    std::unordered_map<core::Income, hgps::LinearModelParams> income_models;
    LinearModelParams model;

    // Open and read CSV file
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open income model CSV: " + csv_path.string());
    }

    try {
        std::string line;
        // Skip header line if it exists (but we don't need to for this file)
        // std::getline(file, line);

        // Process each line
        while (std::getline(file, line)) {
            // Skip empty lines
            if (line.empty()) {
                continue;
            }

            // Check for separator - try tab first, then comma if tab not found
            char separator = '\t';
            size_t sep_pos = line.find(separator);

            // If tab not found, try comma
            if (sep_pos == std::string::npos) {
                separator = ',';
                sep_pos = line.find(separator);
                if (sep_pos == std::string::npos) {
                    std::cout << "\nWarning: Neither tab nor comma found in line: " << line;
                    continue;
                }
            }

            // Extract key and value
            std::string key = line.substr(0, sep_pos);
            std::string value_str = line.substr(sep_pos + 1);

            // Remove any surrounding whitespace and quotes
            key = core::trim(key);
            value_str = core::trim(value_str);

            // Remove quotes if present
            if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
                key = key.substr(1, key.size() - 2);
            }
            if (value_str.size() >= 2 && value_str.front() == '"' && value_str.back() == '"') {
                value_str = value_str.substr(1, value_str.size() - 2);
            }

            if (key.empty() || value_str.empty()) {
                std::cout << "\nWarning: Empty key or value in line: " << line;
                continue;
            }

            // Convert value to double
            double value;
            try {
                value = std::stod(value_str);
            } catch (const std::exception &e) {
                std::cout << "\nWarning: Cannot convert '" << value_str
                          << "' to double: " << e.what();
                continue;
            }

            std::cout << "\nLoaded " << key << " = " << value;

            // Apply value based on parameter name
            if (key == "Intercept") {
                model.intercept = value;
            } else if (key == "stddev") {
                model.coefficients["IncomeContinuousStdDev"] = value;
            } else if (key == "min" || key == "max") {
                model.coefficients[key] = value;
            } else {
                // Map CSV parameter names to the expected names in the code
                std::string mapped_key = key;

                // For gender: gender2 = female (Gender coefficient represents male=0, female=1
                // effect)
                if (key == "gender2") {
                    mapped_key = "Gender";
                }
                // For age: age1 = linear term, age2 = quadratic term
                else if (key == "age1") {
                    mapped_key = "Age";
                } else if (key == "age2") {
                    mapped_key = "Age2";
                }
                // For ethnicity: ethnicity1=White, ethnicity2=Asian, ethnicity3=Black,
                // ethnicity4=Other
                else if (key == "ethnicity2") {
                    mapped_key = "Asian";
                } else if (key == "ethnicity3") {
                    mapped_key = "Black";
                } else if (key == "ethnicity4") {
                    mapped_key = "Others";
                }
                // For region: region1=England, region2=Wales, region3=Scotland, region4=N.Ireland
                else if (key == "region2") {
                    mapped_key = "Wales";
                } else if (key == "region3") {
                    mapped_key = "Scotland";
                } else if (key == "region4") {
                    mapped_key = "NorthernIreland";
                }
                // Income continuous value with "income" and not "Income"
                else if (key == "income") {
                    mapped_key = "income";
                }

                model.coefficients[mapped_key] = value;
            }
        }

        // Add default 0 values for any missing coefficients
        std::vector<std::string> expected_coeffs = {"England", "White", "Mixed", "Sector"};
        for (const auto &coef : expected_coeffs) {
            if (model.coefficients.find(coef) == model.coefficients.end()) {
                model.coefficients[coef] = 0.0;
            }
        }

        // Add the continuous income model
        income_models[core::Income::unknown] = model;

        std::cout << "\nSuccessfully loaded income model with intercept " << model.intercept
                  << " and " << model.coefficients.size() << " coefficients";
    } catch (const std::exception &e) {
        std::cout << "\nFailed to load income model CSV: " << e.what();
        throw;
    }

    return income_models;
}
// NOLINTEND(readability-function-cognitive-complexity)

// NOLINTBEGIN(readability-function-cognitive-complexity)
std::unordered_map<core::Identifier, hgps::LinearModelParams>
load_physical_activity_model_from_csv(const std::filesystem::path &csv_path) {
    std::unordered_map<core::Identifier, hgps::LinearModelParams> physical_activity_models;
    LinearModelParams model;

    // Open and read CSV file
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open physical activity model CSV: " +
                                 csv_path.string());
    }

    try {
        std::string line;
        // Skip header line if it exists (but we don't need to for this file)
        // std::getline(file, line);

        // Process each line
        while (std::getline(file, line)) {
            // Skip empty lines
            if (line.empty()) {
                continue;
            }

            // Check for separator - try tab first, then comma if tab not found
            char separator = '\t';
            size_t sep_pos = line.find(separator);

            // If tab not found, try comma
            if (sep_pos == std::string::npos) {
                separator = ',';
                sep_pos = line.find(separator);
                if (sep_pos == std::string::npos) {
                    std::cout << "\nWarning: Neither tab nor comma found in line: " << line;
                    continue;
                }
            }

            // Extract key and value
            std::string key = line.substr(0, sep_pos);
            std::string value_str = line.substr(sep_pos + 1);

            // Remove any surrounding whitespace and quotes
            key = core::trim(key);
            value_str = core::trim(value_str);

            // Remove quotes if present
            if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
                key = key.substr(1, key.size() - 2);
            }
            if (value_str.size() >= 2 && value_str.front() == '"' && value_str.back() == '"') {
                value_str = value_str.substr(1, value_str.size() - 2);
            }

            if (key.empty() || value_str.empty()) {
                std::cout << "\nWarning: Empty key or value in line: " << line;
                continue;
            }

            // Convert value to double
            double value;
            try {
                value = std::stod(value_str);
            } catch (const std::exception &e) {
                std::cout << "\nWarning: Cannot convert '" << value_str
                          << "' to double: " << e.what();
                continue;
            }

            std::cout << "\nLoaded " << key << " = " << value;

            // Apply value based on parameter name
            if (key == "Intercept") {
                model.intercept = value;
            } else if (key == "stddev") {
                model.coefficients["stddev"] = value;
            } else if (key == "min" || key == "max") {
                model.coefficients[key] = value;
            } else {
                // Map CSV parameter names to the expected names in the code
                std::string mapped_key = key;

                // For gender: gender2 = female (Gender coefficient represents male=0, female=1
                // effect)
                if (key == "gender2") {
                    mapped_key = "Gender";
                }
                // For age: age1 = linear term, age2 = quadratic term
                else if (key == "age1") {
                    mapped_key = "Age";
                } else if (key == "age2") {
                    mapped_key = "Age2";
                }
                // For ethnicity: ethnicity1=White, ethnicity2=Asian, ethnicity3=Black,
                // ethnicity4=Other
                else if (key == "ethnicity2") {
                    mapped_key = "Asian";
                } else if (key == "ethnicity3") {
                    mapped_key = "Black";
                } else if (key == "ethnicity4") {
                    mapped_key = "Others";
                }
                // For region: region1=England, region2=Wales, region3=Scotland, region4=N.Ireland
                else if (key == "region2") {
                    mapped_key = "Wales";
                } else if (key == "region3") {
                    mapped_key = "Scotland";
                } else if (key == "region4") {
                    mapped_key = "NorthernIreland";
                }
                // Income continuous value with "income" and not "Income"
                else if (key == "income") {
                    mapped_key = "income";
                }

                model.coefficients[mapped_key] = value;
            }
        }

        std::cout << "\nSuccessfully loaded physical activity model with intercept "
                  << model.intercept << " and " << model.coefficients.size() << " coefficients";
    } catch (const std::exception &e) {
        std::cout << "\nFailed to load physical activity model CSV: " << e.what();
        throw;
    }

    // Add the model to the map with "continuous" as the key
    physical_activity_models[core::Identifier("continuous")] = model;

    return physical_activity_models;
}
// NOLINTEND(readability-function-cognitive-complexity)
} // namespace hgps::input
