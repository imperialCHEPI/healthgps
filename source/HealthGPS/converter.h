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

		template<std::floating_point TYPE>
		static std::vector<TYPE> create_cdf(std::vector<TYPE>& frequency) {
			auto sum = std::accumulate(std::cbegin(frequency), std::cend(frequency), 0.0f);
			auto pdf = std::vector<float>(frequency.size());
			if (sum > 0) {
				std::transform(std::cbegin(frequency), std::cend(frequency),
					pdf.begin(), [&sum](auto& v) {return v / sum; });
			}
			else {
				auto prob = static_cast<TYPE>(1.0 / (pdf.size() - 1));
				std::fill(pdf.begin(), pdf.end(), prob);
			}

			auto cdf = std::vector<float>(pdf.size());
			cdf[0] = pdf[0];
			for (size_t i = 1; i < pdf.size(); i++) {
				cdf[i] = cdf[i - 1] + pdf[i];
			}

			return cdf;
		}
	}
}