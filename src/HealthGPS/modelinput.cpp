#include "modelinput.h"

#include <utility>

namespace hgps {

ModelInput::ModelInput(core::DataTable &data, Settings settings, const RunInfo &run_info,
                       SESDefinition ses_info, HierarchicalMapping risk_mapping,
                       std::vector<core::DiseaseInfo> diseases, hgps::input::PIFInfo pif_info)
    : input_data_{data}, settings_{std::move(settings)}, run_info_{run_info},
      ses_definition_{std::move(ses_info)}, risk_mapping_{std::move(risk_mapping)},
      diseases_{std::move(diseases)}, pif_info_{std::move(pif_info)} {}

const Settings &ModelInput::settings() const noexcept { return settings_; }

const core::DataTable &ModelInput::data() const noexcept { return input_data_; }

unsigned int ModelInput::start_time() const noexcept { return run_info_.start_time; }

unsigned int ModelInput::stop_time() const noexcept { return run_info_.stop_time; }

unsigned int ModelInput::sync_timeout_ms() const noexcept { return run_info_.sync_timeout_ms; }

const std::optional<unsigned int> &ModelInput::seed() const noexcept { return run_info_.seed; }

const RunInfo &ModelInput::run() const noexcept { return run_info_; }

const SESDefinition &ModelInput::ses_definition() const noexcept { return ses_definition_; }

const HierarchicalMapping &ModelInput::risk_mapping() const noexcept { return risk_mapping_; }

const std::vector<core::DiseaseInfo> &ModelInput::diseases() const noexcept { return diseases_; }

bool ModelInput::enable_income_analysis() const noexcept {
    // Return income analysis flag - forces recompilation
    return enable_income_analysis_;
}

const hgps::input::PIFInfo &ModelInput::population_impact_fraction() const noexcept {
    return pif_info_;
}

} // namespace hgps
