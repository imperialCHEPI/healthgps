#include "modelinput.h"

namespace hgps {
	ModelInput::ModelInput(core::DataTable& data, Settings settings,
		RunInfo run_info, SESMapping ses_mapping)
		: input_data_{data}, settings_{settings},
		  run_info_{run_info}, ses_mapping_{ses_mapping} {
	}

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
}