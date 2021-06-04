#pragma once

#include <vector>
#include "HealthGPS.Core/api.h"

namespace hgps {
	namespace data {

		using namespace hgps::core;

		class Repository
		{
		public:

			virtual ~Repository() = default;

			virtual std::vector<Country> get_countries() = 0;
		};
	}
}
