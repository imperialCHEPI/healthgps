#pragma once

#include <vector>
#include "poco.h"

namespace hgps {
	namespace core {

		class Datastore
		{
		public:
			virtual ~Datastore() = default;

			virtual std::vector<Country> get_countries() = 0;
		};
	}
}