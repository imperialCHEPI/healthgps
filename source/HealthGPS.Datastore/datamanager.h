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

		private:
			const std::filesystem::path root_;
			nlohmann::json index_;
		};
	} 
}
