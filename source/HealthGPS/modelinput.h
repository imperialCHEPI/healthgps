#pragma once

#include <map>
#include <HealthGPS.Core/datatable.h>
#include <HealthGPS.Core/string_util.h>

#include "settings.h"
#include "mapping.h"

namespace hgps {

	struct RunInfo {
		const unsigned int start_time{};
		const unsigned int stop_time{};
		const unsigned int sync_timeout_ms{};
		const std::optional<unsigned int> seed{};
	};

	struct SESDefinition {
		const std::string fuction_name;
		const std::vector<double> parameters;
	};

	class ModelInput
	{
	public:
		ModelInput() = delete;
		ModelInput(core::DataTable& data, const Settings& settings, const RunInfo& run_info, 
			const SESDefinition& ses_info, const HierarchicalMapping& risk_mapping, 
			const std::vector<core::DiseaseInfo>& diseases);

		const Settings& settings() const noexcept;

		const core::DataTable& data() const noexcept;

		const unsigned int& start_time() const noexcept;

		const unsigned int& stop_time() const noexcept;

		const unsigned int& sync_timeout_ms() const noexcept;

		const std::optional<unsigned int>& seed() const noexcept;

		const RunInfo& run() const noexcept;

		const SESDefinition& ses_definition() const noexcept;

		const HierarchicalMapping& risk_mapping() const noexcept;

		const std::vector<core::DiseaseInfo>& diseases() const noexcept;

	private:
		std::reference_wrapper<core::DataTable> input_data_;
		Settings settings_;
		RunInfo run_info_;
		SESDefinition ses_definition_;
		HierarchicalMapping risk_mapping_;
		std::vector<core::DiseaseInfo> diseases_;
	};
}