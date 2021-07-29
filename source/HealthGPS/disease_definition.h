#pragma once

#include "disease_table.h"
#include "relative_risk.h"
#include "HealthGPS.Core\poco.h"

namespace hgps {

	class DiseaseDefinition {
	public:
		DiseaseDefinition(DiseaseTable&& measures_table,
			RelativeRiskTableMap&& diseases, RelativeRiskLookupMap&& risk_factors)
			: measures_table_{ measures_table }, relative_risk_diseases_{ diseases },
			relative_risk_factors_{ risk_factors } {}

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

	private:
		DiseaseTable measures_table_;
		RelativeRiskTableMap relative_risk_diseases_;
		RelativeRiskLookupMap relative_risk_factors_;
	};
}
