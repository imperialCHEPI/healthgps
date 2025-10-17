#include "repository.h"
#include "HealthGPS.Input/datamanager.h"
#include "HealthGPS.Input/pif_data.h"
#include "converter.h"

#include <fmt/core.h>
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
    region_prevalence_.clear();
    ethnicity_prevalence_.clear();
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

    // Load PIF data if available
    hgps::input::PIFData pif_data;
    if (config.population_impact_fraction().enabled) {
        // Convert PIFInfo to JSON for DataManager, using the DataManager's root directory
        nlohmann::json pif_config;
        pif_config["enabled"] = config.population_impact_fraction().enabled;
        // Use the DataManager's root directory instead of requiring environment variable
        pif_config["data_root_path"] =
            dynamic_cast<hgps::input::DataManager &>(data_manager_.get()).get_root_path();
        pif_config["risk_factor"] = config.population_impact_fraction().risk_factor;
        pif_config["scenario"] = config.population_impact_fraction().scenario;

        auto pif_result = dynamic_cast<hgps::input::DataManager &>(data_manager_.get())
                              .get_pif_data(info, config.settings().country(), pif_config);
        if (pif_result.has_value()) {
            pif_data = std::move(pif_result.value());
        }
    }

    if (info.group != core::DiseaseGroup::cancer) {
        auto definition =
            DiseaseDefinition(std::move(disease_table), std::move(relative_risks.diseases),
                              std::move(relative_risks.risk_factors), std::move(pif_data));
        diseases_.emplace(info.code, definition);
    } else {
        auto cancer_param =
            data_manager_.get().get_disease_parameter(info, config.settings().country());
        auto parameter = detail::StoreConverter::to_disease_parameter(cancer_param);

        auto definition = DiseaseDefinition(
            std::move(disease_table), std::move(relative_risks.diseases),
            std::move(relative_risks.risk_factors), std::move(parameter), std::move(pif_data));
        diseases_.emplace(info.code, definition);
    }
}

void CachedRepository::register_region_prevalence(
    const std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>>
        &region_data) {
    std::unique_lock<std::mutex> lock(mutex_);
    region_prevalence_ = region_data;
}

void CachedRepository::register_ethnicity_prevalence(
    const std::map<core::Identifier,
                   std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>>
        &ethnicity_data) {
    std::unique_lock<std::mutex> lock(mutex_);
    ethnicity_prevalence_ = ethnicity_data;
}

const std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>> &
CachedRepository::get_region_prevalence() const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return region_prevalence_;
}

const std::map<core::Identifier,
               std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>> &
CachedRepository::get_ethnicity_prevalence() const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return ethnicity_prevalence_;
}
} // namespace hgps
