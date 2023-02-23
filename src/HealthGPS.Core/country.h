#pragma once
#include <string>

namespace hgps {
	namespace core {

		/// @brief Country ISO-3166 definition data structure
		struct Country
		{
			/// @brief Unique code identifier, e.g. 250
			int code{};

			/// @brief Full country name, e.g. France
			std::string name{};

			/// @brief The alpha 2 characters unique identifier, e.g., FR
			std::string alpha2{};

			/// @brief The alpha 3 characters unique identifier, e.g., FRA
			std::string alpha3{};
		};

		inline bool operator> (const Country& lhs, const Country& rhs) { return lhs.name > rhs.name; }
		inline bool operator< (const Country& lhs, const Country& rhs) { return lhs.name < rhs.name; }
	}
}
