#pragma once
#include <string>

namespace hgps {
	namespace core {

		/// @brief Trim leading and trailing occurrences of white-space characters from string.
		std::string trim(std::string value);

		/// @brief 
		std::string to_lower(std::string_view value);

		std::string to_upper(std::string_view value);

		struct case_insensitive {
			struct comparator {
				bool operator() (const std::string& s1, const std::string& s2) const;
			};

			static bool equals(const std::string_view& left, const std::string_view& right);
		};
	}
}