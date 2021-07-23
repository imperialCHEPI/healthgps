#pragma once

#include <vector>
#include <optional>
#include "interval.h"
#include "poco.h"

namespace hgps {
	namespace core {

		class Datastore
		{
		public:
			virtual ~Datastore() = default;

			virtual std::vector<Country> get_countries() const = 0;

			virtual std::optional<Country> get_country(std::string alpha) const = 0;

			virtual std::vector<PopulationItem> get_population(
				Country country, const std::function<bool(const unsigned int&)> year_filter) const = 0;

			virtual std::vector<DiseaseInfo> get_diseases() const = 0;

			virtual std::optional<DiseaseInfo> get_disease_info(std::string code) const = 0;

			virtual DiseaseEntity get_disease(DiseaseInfo info, Country country) const = 0;

			virtual RelativeRiskTable get_relative_risk_to_disease(
				DiseaseInfo source, DiseaseInfo target) const = 0;

			virtual RelativeRiskTable get_relative_risk_to_risk_factor(
				DiseaseInfo source, Gender gender, std::string risk_factor) const = 0;
		};
	}
}