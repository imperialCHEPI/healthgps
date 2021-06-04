#pragma once

#include "HealthGPS.Core/api.h"
#include "repository.h"

namespace hgps {
	namespace data {

		using namespace hgps::core;

		class DataManager : public Datastore
		{
		public:
			explicit DataManager(Repository& provider);
			~DataManager();

			DataManager() = delete;

			std::vector<Country> get_countries();

		private:
			Repository& source_;
		};
	} 
}
