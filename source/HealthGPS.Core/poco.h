#pragma once

#include <string>

namespace hgps {
	namespace core {

		struct Country
		{
			std::string code{};

			std::string name{};

			bool operator > (const Country& other) const { return (name > other.name); }
			bool operator < (const Country& other) const { return (name < other.name); }
		};
	}
}