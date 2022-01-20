#pragma once

#include "disease_table.h"
#include "relative_risk.h"
#include "gender_value.h"
#include "HealthGPS.Core/poco.h"

namespace hgps {

	using ParameterLookup = const std::map<int, DoubleGenderValue>;

	struct DiseaseParameter final {
		DiseaseParameter() = default;
		DiseaseParameter(const int data_time, ParameterLookup& prevalence,
			ParameterLookup& survival, ParameterLookup& deaths)
			: time_year{ data_time }, prevalence_distribution{ prevalence },
			survival_rate{ survival }, death_weight{ deaths },
			max_time_since_onset{ prevalence.rbegin()->first + 1 } {}

		const int time_year{};
		const ParameterLookup prevalence_distribution{};
		const ParameterLookup survival_rate{};
		const ParameterLookup death_weight{};
		const int max_time_since_onset{};
	};

	class DiseaseDefinition final {
	public:
		DiseaseDefinition(DiseaseTable&& measures_table, RelativeRiskTableMap&& diseases,
			RelativeRiskLookupMap&& risk_factors)
			: measures_table_{ std::move(measures_table) }, relative_risk_diseases_{ std::move(diseases) },
			relative_risk_factors_{ std::move(risk_factors) }, parameters_{} {}

		DiseaseDefinition(DiseaseTable&& measures_table, RelativeRiskTableMap&& diseases,
			RelativeRiskLookupMap&& risk_factors, DiseaseParameter&& parameter)
			: measures_table_{ std::move(measures_table) }, relative_risk_diseases_{ std::move(diseases) },
			relative_risk_factors_{ std::move(risk_factors) }, parameters_{parameter} {}

		const core::DiseaseInfo& identifier() const noexcept {
			return measures_table_.info();
		}

		const DiseaseTable& table() const noexcept {
			return measures_table_;
		}

		const RelativeRiskTableMap& relative_risk_diseases() const noexcept {
			return relative_risk_diseases_;
		}

		const RelativeRiskLookupMap& relative_risk_factors() const noexcept {
			return relative_risk_factors_;
		}

		const DiseaseParameter& parameters() const noexcept {
			return parameters_;
		}

	private:
		DiseaseTable measures_table_;
		RelativeRiskTableMap relative_risk_diseases_;
		RelativeRiskLookupMap relative_risk_factors_;
		DiseaseParameter parameters_;
	};
}
