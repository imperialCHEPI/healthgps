#pragma once

#include <map>
#include <HealthGPS.Core/datatable.h>
#include <HealthGPS.Core/string_util.h>

#include "settings.h"
#include "mapping.h"

namespace hgps {

	struct RunInfo {
		const unsigned int start_time;
		const unsigned int stop_time;
		const unsigned int sync_timeout_ms;
		const std::optional<unsigned int> seed;
	};

	struct SESMapping {
		const unsigned int update_interval;
		const unsigned int update_max_age;
		const std::map<std::string, std::string> entries;
	};

	class ModelInput
	{
	public:
		ModelInput() = delete;
		ModelInput(core::DataTable& data, Settings settings, RunInfo info, 
			SESMapping ses_mapping, HierarchicalMapping risk_mapping, 
			std::vector<core::DiseaseInfo> diseases);

		const Settings& settings() const noexcept;

		const core::DataTable& data() const noexcept;

		const unsigned int& start_time() const noexcept;

		const unsigned int& stop_time() const noexcept;

		const unsigned int& sync_timeout_ms() const noexcept;

		const std::optional<unsigned int>& seed() const noexcept;

		const RunInfo& run() const noexcept;

		const SESMapping& ses_mapping() const noexcept;

		const HierarchicalMapping& risk_mapping() const noexcept;

		const std::vector<core::DiseaseInfo>& diseases() const noexcept;

	private:
		core::DataTable& input_data_;
		Settings settings_;
		RunInfo run_info_;
		SESMapping ses_mapping_;
		HierarchicalMapping risk_mapping_;
		std::vector<core::DiseaseInfo> diseases_;
	};
}