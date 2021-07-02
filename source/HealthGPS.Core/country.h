#pragma once
#include <string>

namespace hgps {
	namespace core {

		struct Country
		{
			int code{};
			std::string name{};
			std::string alpha2{};
			std::string alpha3{};
		};

		inline bool operator> (const Country& lhs, const Country& rhs) { return lhs.name > rhs.name; }
		inline bool operator< (const Country& lhs, const Country& rhs) { return lhs.name < rhs.name; }
	}
}
