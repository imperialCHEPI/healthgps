#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>

#include "HealthGPS.Core/api.h"

namespace hgps {
	namespace data {

		using namespace hgps::core;

		class DataManager : public Datastore
		{
		public:
			DataManager() = delete;
			explicit DataManager(const std::filesystem::path root_directory);

			std::vector<Country> get_countries() const override;

			std::optional<Country> get_country(std::string alpha) const override;

			std::vector<PopulationItem> get_population(Country country) const;

			std::vector<PopulationItem> get_population(
				Country country, const std::function<bool(const unsigned int&)> year_filter) const override;

			std::vector<DiseaseInfo> get_diseases() const override;

			std::optional<DiseaseInfo> get_disease_info(std::string code) const override;

			DiseaseEntity get_disease(DiseaseInfo code, Country country) const override;

			RelativeRiskTable get_relative_risk_to_disease(
				DiseaseInfo source, DiseaseInfo target) const override;

			RelativeRiskTable get_relative_risk_to_risk_factor(
				DiseaseInfo source, Gender gender, std::string risk_factor) const override;

		private:
			const std::filesystem::path root_;
			nlohmann::json index_;

			RelativeRiskTable generate_default_relative_risk_to_disease() const;

			std::string replace_string_tokens(std::string source, std::vector<std::string> tokens) const;
		};
	} 
}
