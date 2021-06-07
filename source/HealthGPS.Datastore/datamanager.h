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

			std::vector<Country> get_countries();

		private:
			const std::filesystem::path root_;
			nlohmann::json index_;
		};
	} 
}
