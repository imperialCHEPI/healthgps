#pragma once

#include <map>
#include <HealthGPS.Core/datatable.h>
#include <HealthGPS.Core/string_util.h>
#include "population.h"

namespace hgps {

	struct RunInfo {
		unsigned int start_time;
		unsigned int stop_time;
		std::optional<unsigned int> seed;
	};

	struct SESMapping {
		std::map<std::string, std::string, core::case_insensitive::comparator> entries;
	};

	class ModelInput
	{
	public:
		ModelInput() = delete;
		ModelInput(core::DataTable& data, Population population,
			RunInfo info, SESMapping ses_mapping);

		Population population() const noexcept;

		core::DataTable& data() const noexcept;

		unsigned int start_time() const noexcept;

		unsigned int stop_time() const noexcept;

		std::optional<unsigned int> seed() const noexcept;

		SESMapping ses_mapping() const noexcept;

	private:
		core::DataTable& input_data_;
		const Population population_;
		const RunInfo run_info_;
		const SESMapping ses_mapping_;
	};
}