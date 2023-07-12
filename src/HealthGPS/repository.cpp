#include "repository.h"
#include "converter.h"

#include <fmt/core.h>
#include <stdexcept>

namespace hgps {

CachedRepository::CachedRepository(core::Datastore &manager)
    : mutex_{}, data_manager_{manager}, rf_model_definition_{}, baseline_adjustments_{},
      diseases_info_{}, diseases_{}, lms_parameters_{} {}

void CachedRepository::register_risk_factor_model_definition(
    const HierarchicalModelType &model_type,
    std::unique_ptr<RiskFactorModelDefinition> definition) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (rf_model_definition_.contains(model_type)) {
        rf_model_definition_.erase(model_type);
    }

    rf_model_definition_.emplace(model_type, std::move(definition));
}

void CachedRepository::register_baseline_adjustment_definition(BaselineAdjustment definition) {
    std::unique_lock<std::mutex> lock(mutex_);
    baseline_adjustments_ = std::move(definition);
}

core::Datastore &CachedRepository::manager() noexcept { return data_manager_; }

const RiskFactorModelDefinition &
CachedRepository::get_risk_factor_model_definition(const HierarchicalModelType &model_type) const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return *rf_model_definition_.at(model_type);
}

BaselineAdjustment &CachedRepository::get_baseline_adjustment_definition() {
    std::scoped_lock<std::mutex> lock(mutex_);
    return baseline_adjustments_;
}

const std::vector<core::DiseaseInfo> &CachedRepository::get_diseases() {
    std::scoped_lock<std::mutex> lock(mutex_);
    if (diseases_info_.empty()) {
        diseases_info_ = data_manager_.get().get_diseases();
    }

    return diseases_info_;
}

std::optional<core::DiseaseInfo> CachedRepository::get_disease_info(core::Identifier code) {
    auto &all_diseases = get_diseases();
    auto it = std::find_if(all_diseases.cbegin(), all_diseases.cend(),
                           [&code](const core::DiseaseInfo &other) { return other.code == code; });

    if (it != all_diseases.cend()) {
        return *it;
    }

    return std::optional<core::DiseaseInfo>();
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
} // namespace hgps
