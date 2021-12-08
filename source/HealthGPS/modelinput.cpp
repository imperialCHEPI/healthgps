#include "modelinput.h"

namespace hgps {
	ModelInput::ModelInput(core::DataTable& data, Settings settings, RunInfo run_info,
		SESDefinition ses_info, HierarchicalMapping risk_mapping, 
		std::vector<core::DiseaseInfo> diseases)
		: input_data_{data}, settings_{settings},
		run_info_{ run_info }, ses_definition_{ ses_info },
		risk_mapping_{ risk_mapping }, diseases_{diseases} { }

	const Settings& ModelInput::settings() const noexcept {
		return settings_;
	}

	const core::DataTable& ModelInput::data() const noexcept {
		return input_data_;
	}

	const unsigned int& ModelInput::start_time() const noexcept {
		return run_info_.start_time;
	}

	const unsigned int& ModelInput::stop_time() const noexcept {
		return run_info_.stop_time;
	}

	const unsigned int& ModelInput::sync_timeout_ms() const noexcept {
		return run_info_.sync_timeout_ms;
	}

	const std::optional<unsigned int>& ModelInput::seed() const noexcept {
		return run_info_.seed;
	}

	const RunInfo& ModelInput::run() const noexcept {
		return run_info_;
	}

	const SESDefinition& ModelInput::ses_definition() const noexcept {
		return ses_definition_;
	}

	const HierarchicalMapping& ModelInput::risk_mapping() const noexcept {
		return risk_mapping_;
	}

	const std::vector<core::DiseaseInfo>& ModelInput::diseases() const noexcept {
		return diseases_;
	}
}