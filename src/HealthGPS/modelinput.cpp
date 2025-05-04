#include "modelinput.h"

#include <utility>

namespace hgps {

ModelInput::ModelInput(core::DataTable data, Settings settings, const RunInfo &run_info,
                       SESDefinition ses_info, HierarchicalMapping risk_mapping,
                       std::vector<core::DiseaseInfo> diseases)
    : input_data_{std::move(data)}, settings_{std::move(settings)}, run_info_{run_info},
      ses_definition_{std::move(ses_info)}, risk_mapping_{std::move(risk_mapping)},
      diseases_{std::move(diseases)} {}

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

std::unordered_map<core::Region, double>
ModelInput::get_region_probabilities(int age, core::Gender gender) const {
    // Get probabilities from data table using proper distribution method
    return input_data_.get_region_distribution(age, gender);
}

std::unordered_map<core::Ethnicity, double>
ModelInput::get_ethnicity_probabilities(int age, core::Gender gender, core::Region region) const {
    // Get probabilities from the input data table using proper distribution method
    return input_data_.get_ethnicity_distribution(age, gender, region);
}
} // namespace hgps
