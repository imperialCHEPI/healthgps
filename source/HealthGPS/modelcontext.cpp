#include "modelcontext.h"

namespace hgps {
	ModelContext::ModelContext(core::DataTable& data, Population population,
		RunInfo run_info, SESMapping ses_mapping)
		: input_data_{data}, population_{population},
		  run_info_{run_info}, ses_mapping_{ses_mapping} {
	}

	Population ModelContext::population() const noexcept {
		return population_;
	}

	core::DataTable& ModelContext::data() const noexcept {
		return input_data_;
	}

	RunInfo ModelContext::run_info() const noexcept	{
		return run_info_;
	}

	SESMapping ModelContext::ses_mapping() const noexcept {
		return ses_mapping_;
	}
}