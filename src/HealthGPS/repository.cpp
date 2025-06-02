#include "repository.h"
#include "converter.h"

#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace hgps {

CachedRepository::CachedRepository(core::Datastore &manager) : data_manager_{manager} {}

void CachedRepository::register_risk_factor_model_definition(
    const RiskFactorModelType &model_type, std::unique_ptr<RiskFactorModelDefinition> definition) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (rf_model_definition_.contains(model_type)) {
        rf_model_definition_.erase(model_type);
    }

    rf_model_definition_.emplace(model_type, std::move(definition));
}

core::Datastore &CachedRepository::manager() noexcept { return data_manager_; }

const RiskFactorModelDefinition &
CachedRepository::get_risk_factor_model_definition(const RiskFactorModelType &model_type) const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return *rf_model_definition_.at(model_type);
}

const std::vector<core::DiseaseInfo> &CachedRepository::get_diseases() {
    std::scoped_lock<std::mutex> lock(mutex_);
    if (diseases_info_.empty()) {
        diseases_info_ = data_manager_.get().get_diseases();
    }

    return diseases_info_;
}

std::optional<core::DiseaseInfo> CachedRepository::get_disease_info(core::Identifier code) {
    const auto &all_diseases = get_diseases();
    auto it = std::find_if(all_diseases.cbegin(), all_diseases.cend(),
                           [&code](const core::DiseaseInfo &other) { return other.code == code; });

    if (it != all_diseases.cend()) {
        return *it;
    }

    return {};
}

DiseaseDefinition &CachedRepository::get_disease_definition(const core::DiseaseInfo &info,
                                                            const ModelInput &config) {

    // lock-free multiple readers
    if (diseases_.contains(info.code)) {
        return diseases_.at(info.code);
    }

    std::scoped_lock<std::mutex> lock(mutex_);
    try {
        load_disease_definition(info, config);
        return diseases_.at(info.code);
    } catch (const std::exception &ex) {
        auto code_str = info.code.to_string();
        throw std::runtime_error(
            fmt::format("Filed to load disease {} definition, {}", code_str, ex.what()));
    }
}

LmsDefinition &CachedRepository::get_lms_definition() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (lms_parameters_.empty()) {
        auto lms_poco = data_manager_.get().get_lms_parameters();
        lms_parameters_ = detail::StoreConverter::to_lms_definition(lms_poco);
    }

    return lms_parameters_;
}

void CachedRepository::clear_cache() noexcept {
    std::unique_lock<std::mutex> lock(mutex_);
    rf_model_definition_.clear();
    diseases_info_.clear();
    diseases_.clear();
}

void CachedRepository::load_disease_definition(const core::DiseaseInfo &info,
                                               const ModelInput &config) {
    if (diseases_.contains(info.code)) {
        return;
    }

    auto risk_factors = std::vector<MappingEntry>();
    for (auto level = 1; level <= config.risk_mapping().max_level(); level++) {
        auto risks = config.risk_mapping().at_level(level);
        risk_factors.insert(risk_factors.end(), risks.begin(), risks.end());
    }

    auto disease_entity = data_manager_.get().get_disease(info, config.settings().country());
    auto disease_table = detail::StoreConverter::to_disease_table(disease_entity);
    auto relative_risks =
        detail::create_relative_risk(detail::RelativeRiskInfo{.disease = info,
                                                              .manager = data_manager_.get(),
                                                              .inputs = config,
                                                              .risk_factors = risk_factors});

    if (info.group != core::DiseaseGroup::cancer) {
        auto definition =
            DiseaseDefinition(std::move(disease_table), std::move(relative_risks.diseases),
                              std::move(relative_risks.risk_factors));
        diseases_.emplace(info.code, definition);
    } else {
        auto cancer_param =
            data_manager_.get().get_disease_parameter(info, config.settings().country());
        auto parameter = detail::StoreConverter::to_disease_parameter(cancer_param);

        auto definition =
            DiseaseDefinition(std::move(disease_table), std::move(relative_risks.diseases),
                              std::move(relative_risks.risk_factors), std::move(parameter));
        diseases_.emplace(info.code, definition);
    }
}

std::unordered_map<core::Identifier, LinearModelParams>
CachedRepository::get_systolic_blood_pressure_models() const {
    std::unordered_map<core::Identifier, LinearModelParams> models;
    LinearModelParams model;

    // Open and read CSV file
    std::ifstream file;
    file.open(root_path_ / "systolicbloodpressure.csv");
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open systolic blood pressure model CSV: " +
                                 (root_path_ / "systolicbloodpressure.csv").string());
    }

    try {
        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines
            if (line.empty()) {
                continue;
            }

            // Split line by comma
            std::stringstream ss(line);
            std::string key, value_str;
            if (!std::getline(ss, key, ',') || !std::getline(ss, value_str, ',')) {
                continue;
            }

            // Trim whitespace
            key = core::trim(key);
            value_str = core::trim(value_str);

            // Convert value to double
            double value = std::stod(value_str);

            // Map parameter names
            if (key == "Intercept") {
                model.intercept = value;
            } else if (key == "stddev") {
                model.coefficients["stddev"] = value;
            } else if (key == "min" || key == "max") {
                model.coefficients[key] = value;
            } else {
                // Map CSV parameter names to the expected names in the code
                std::string mapped_key = key;
                if (key == "gender2")
                    mapped_key = "Gender";
                else if (key == "age1")
                    mapped_key = "Age";
                else if (key == "age2")
                    mapped_key = "Age2";
                else if (key == "age3")
                    mapped_key = "Age3";
                else if (key == "ethnicity2")
                    mapped_key = "Asian";
                else if (key == "ethnicity3")
                    mapped_key = "Black";
                else if (key == "ethnicity4")
                    mapped_key = "Others";
                else if (key == "region2")
                    mapped_key = "Wales";
                else if (key == "region3")
                    mapped_key = "Scotland";
                else if (key == "region4")
                    mapped_key = "NorthernIreland";
                else if (key == "bmi")
                    mapped_key = "BMI";
                else if (key == "bloodpressuremedication")
                    mapped_key = "BloodPressureMedication";

                model.coefficients[mapped_key] = value;
            }
        }

        std::cout << "\nSuccessfully loaded systolic blood pressure model with intercept "
                  << model.intercept << " and " << model.coefficients.size() << " coefficients";
    } catch (const std::exception &e) {
        std::cout << "\nFailed to load systolic blood pressure model CSV: " << e.what();
        throw;
    }

    // Add the model directly to systolicbloodpressure key
    models[core::Identifier("systolicbloodpressure")] = model;
    return models;
}
} // namespace hgps
