#pragma once
#include <string>
#include <functional>

namespace hgps {
	namespace core {

		/// @brief Trim leading and trailing occurrences of white-space characters from string.
		std::string trim(std::string value);

		std::string to_lower(std::string_view value);

		std::string to_upper(std::string_view value);

		std::vector<std::string_view> split_string(std::string_view value, std::string_view delims);

		struct case_insensitive {
			struct comparator {
				bool operator() (const std::string& left, const std::string& right) const;
			};

			static std::weak_ordering compare(const std::string& left, const std::string& right);

			static bool equals(const std::string_view& left, const std::string_view& right);

			static bool contains(const std::string_view& text, const std::string_view& str);

			static bool starts_with(const std::string_view& text, const std::string_view& str);

			static bool ends_with(const std::string_view& text, const std::string_view& str);
		};
	}
}