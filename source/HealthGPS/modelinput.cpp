#include "modelinput.h"

namespace hgps {
	ModelInput::ModelInput(core::DataTable& data, Settings settings, RunInfo run_info,
		SESMapping ses_mapping, HierarchicalMapping risk_mapping, 
		std::vector<core::DiseaseInfo> diseases)
		: input_data_{data}, settings_{settings},
		run_info_{ run_info }, ses_mapping_{ ses_mapping },
		risk_mapping_{ risk_mapping }, diseases_{diseases} { }

	Settings ModelInput::settings() const noexcept {
		return settings_;
	}

	core::DataTable& ModelInput::data() const noexcept {
		return input_data_;
	}

	unsigned int ModelInput::start_time() const noexcept {
		return run_info_.start_time;
	}

	unsigned int ModelInput::stop_time() const noexcept {
		return run_info_.stop_time;
	}

	std::optional<unsigned int> ModelInput::seed() const noexcept {
		return run_info_.seed;
	}

	SESMapping ModelInput::ses_mapping() const noexcept {
		return ses_mapping_;
	}

	HierarchicalMapping ModelInput::risk_mapping() const noexcept {
		return risk_mapping_;
	}
	std::vector<core::DiseaseInfo> ModelInput::diseases() const noexcept {
		return diseases_;
	}
}