#pragma once
#include "modelinput.h"
#include "disease_table.h"
#include "relative_risk.h"
#include "gender_table.h"
#include "analysis_definition.h"
#include "life_table.h"

#include "HealthGPS.Core/poco.h"
#include "HealthGPS.Core/DataStore.h"

namespace hgps {
	namespace detail {

		struct RelativeRiskInfo
		{
			core::DiseaseInfo& disease;

			core::Datastore& manager;

			ModelInput& inputs;

			std::vector<MappingEntry>& risk_factors;
		};

		class StoreConverter
		{
		public:
			StoreConverter() = delete;

			static core::Gender to_gender(std::string name);

			static DiseaseTable to_disease_table(const core::DiseaseEntity& entity);

			static FloatAgeGenderTable to_relative_risk_table(const core::RelativeRiskEntity& entity);

			static RelativeRiskLookup to_relative_risk_lookup(const core::RelativeRiskEntity& entity);

			static AnalysisDefinition to_analysis_definition(
				const core::DiseaseAnalysisEntity& entity, const core::IntegerInterval& age_range);

			static LifeTable to_life_table(std::vector<core::BirthItem>& births, std::vector<core::MortalityItem>& deaths);
		};

		RelativeRisk create_relative_risk(RelativeRiskInfo info);
	}
}