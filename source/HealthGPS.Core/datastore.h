#pragma once

#include <vector>
#include <optional>
#include "poco.h"

namespace hgps {
	namespace core {

		class Datastore
		{
		public:
			virtual ~Datastore() = default;

			virtual std::vector<Country> get_countries() const = 0;

			virtual std::optional<Country> get_country(std::string code) const = 0;
		};
	}
}