#include "modelinput.h"

namespace hgps {
	ModelInput::ModelInput(core::DataTable& data, Population population,
		RunInfo run_info, SESMapping ses_mapping)
		: input_data_{data}, population_{population},
		  run_info_{run_info}, ses_mapping_{ses_mapping} {
	}

	Population ModelInput::population() const noexcept {
		return population_;
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