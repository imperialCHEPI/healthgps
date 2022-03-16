#pragma once
#include "modelinput.h"
#include "disease_table.h"
#include "relative_risk.h"
#include "gender_table.h"
#include "analysis_definition.h"
#include "disease_definition.h"
#include "life_table.h"

#include "HealthGPS.Core/poco.h"
#include "HealthGPS.Core/datastore.h"

namespace hgps {
	namespace detail {

		struct RelativeRiskInfo
		{
			const core::DiseaseInfo& disease;

			core::Datastore& manager;

			const ModelInput& inputs;

			std::vector<MappingEntry>& risk_factors;
		};

		class StoreConverter
		{
		public:
			StoreConverter() = delete;

			static core::Gender to_gender(const std::string name);

			static DiseaseTable to_disease_table(const core::DiseaseEntity& entity);

			static FloatAgeGenderTable to_relative_risk_table(const core::RelativeRiskEntity& entity);

			static RelativeRiskLookup to_relative_risk_lookup(const core::RelativeRiskEntity& entity);

			static AnalysisDefinition to_analysis_definition(const core::DiseaseAnalysisEntity& entity);

			static LifeTable to_life_table(const std::vector<core::BirthItem>& births,
				const std::vector<core::MortalityItem>& deaths);

			static DiseaseParameter to_disease_parameter(const core::CancerParameterEntity& entity);
		};

		RelativeRisk create_relative_risk(const RelativeRiskInfo& info);

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